#!/usr/bin/env python3
"""Keep the illustrative Python lab aligned with production log/reset policy."""
import datetime
import importlib.util
import pathlib
import unittest
from unittest.mock import patch
SOURCE = pathlib.Path(__file__).resolve().parents[1] / "Simulator/lab/server.py"
spec = importlib.util.spec_from_file_location("user_lab", SOURCE)
lab = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lab)
NOW = 1793836800
class LogPolicy(unittest.TestCase):
    def setUp(self):
        self.clock = patch.object(lab.time, "time", return_value=NOW)
        self.clock.start()
        self.addCleanup(self.clock.stop)
        self.board = lab.Board()
        self.day = self.board.today()
        self.marker = self.board.row(NOW, 0, 7)
    def test_preview_calendar_isolation(self):
        raw = lab.load_panel_html()
        preview = lab.load_preview_html()
        self.assertEqual(preview, raw)
        self.assertIn('/calendar.js', raw)
        script = (SOURCE.parent.parent.parent / "charge_calendar.h").read_text(encoding="utf-8")
        expected = script.split('R"calendar(', 1)[1].split(')calendar"', 1)[0]
        self.assertEqual(lab.load_calendar_js(), expected)
    def test_about_uses_shared_compiled_content(self):
        about = lab.load_about_html()
        self.assertIn("09367969890", about)
        self.assertIn("تهران-تهران", about)
        self.assertIn("<bdi></bdi>", about)
        self.assertNotIn("09367969890", lab.load_panel_html())
    def test_simulated_charge_start_and_completion(self):
        events = self.board.make_charges(NOW)
        self.assertTrue(events)
        for event in events:
            self.assertEqual(event["ts"] - event["start"], event["dur"])
            self.assertLessEqual(event["ts"], NOW)
            self.assertEqual((event["start"] // 3600) % self.board.CYCLE_HOURS, 0)
        # An in-progress charging window is not a completed charge yet.
        start = events[-1]["start"] + self.board.CYCLE_HOURS * 3600
        later = self.board.make_charges(start + 3600)
        self.assertFalse(any(event["start"] == start and event["status"] == "complete" for event in later))
        self.assertTrue({"complete", "partial"} <= {event["status"] for event in events})
        self.assertEqual(self.board.charge_count, sum(e["status"] == "complete" for e in self.board.charge_history))
        active = self.board.make_charges(start + 60)
        self.assertTrue(any(event["start"] == start and event["status"] == "charging" for event in active))
    def test_calendar_retention(self):
        def day(age):
            return (datetime.date.fromtimestamp(NOW) - datetime.timedelta(days=age)).strftime("%Y%m%d")
        self.board.rows = {day(age): [self.marker] for age in (0, 29, 30, 60)}
        self.board.trim_logs()
        self.assertEqual(sorted(self.board.rows), sorted((day(0), day(29))))
    def test_207_keeps_newest_200_and_start_marker(self):
        rows = [self.board.row(NOW + i, 0, 0) for i in range(1, 208)]
        self.board.rows = {self.day: [self.marker] + rows}
        self.board.trim_logs()
        self.assertEqual(self.board.rows[self.day], [self.marker] + rows[7:])
    def test_minimal_byte_eviction(self):
        rows = []
        value = 17
        for index in range(120):
            fields = []
            for _ in range(18):
                value = (value * 1664525 + 1013904223) & 0xffffffff
                fields.append(value)
            fields[0], fields[3] = NOW + index, 0
            rows.append(",".join(map(str, fields)))
        first = 0
        while self.board.encoded_day_bytes([self.marker] + rows[first:]) > 4096:
            first += 1
        self.board.rows = {self.day: [self.marker] + rows}
        self.board.trim_logs()
        self.assertEqual(self.board.rows[self.day], [self.marker] + rows[first:])
        self.assertLessEqual(self.board.encoded_day_bytes(self.board.rows[self.day]), 4096)
        self.assertGreater(self.board.encoded_day_bytes([self.marker] + rows[first-1:]), 4096)
    def test_new_day_starts_once(self):
        self.board.rows = {}
        self.board.append_log(2, 0)
        self.board.append_log(16, 0)
        self.assertEqual([row.split(",")[3] for row in self.board.rows[self.day]], ["7", "0", "0"])
    def test_reset_contract(self):
        class Request(lab.Handler):
            def __init__(self, args, admin):
                self.path, self.args, self.admin = "/latch/reset", args, admin
            def body_args(self):
                return self.args
            def session_ok(self):
                return self.admin
            def send(self, result):
                self.result = result
        for admin in (False, True):
            for n in (None, "", "5junk", "6junk", "8junk", "05", "+5", " 5", "5 ", "5.0", "7", "۵", "5", "6", "8"):
                with self.subTest(admin=admin, n=n):
                    before = dict(lab.BOARD.latch)
                    request = Request({} if n is None else {"n": n}, admin)
                    request.route("POST")
                    status, _, raw = request.result
                    body = lab.json.loads(raw)
                    expected = 403 if not admin else 200 if n in ("5", "6", "8") else 400
                    self.assertEqual(status, expected)
                    if status == 400:
                        self.assertEqual(body, {"ok": 0, "e": "reset", "allowed": [5, 6, 8]})
                    if status == 200:
                        self.assertEqual(body, {"ok": 1, "n": int(n)})
                    else:
                        self.assertEqual(lab.BOARD.latch, before)
    def test_factory_panel_only(self):
        class Request(lab.Handler):
            def __init__(self, args, admin):
                self.path, self.args, self.admin = "/factory/reset", args, admin
            def body_args(self):
                return self.args
            def session_ok(self):
                return self.admin
            def send(self, result):
                self.result = result
        old = lab.BOARD
        self.addCleanup(setattr, lab, "BOARD", old)
        lab.BOARD = self.board
        before = (self.board.charge_counts[:], self.board.board_uptime, dict(self.board.latch))
        for args, admin, expected in (({}, False, 403), ({}, True, 400), ({"confirm": "wrong"}, True, 400)):
            req = Request(args, admin)
            req.route("POST")
            self.assertEqual(req.result[0], expected)
            self.assertTrue(self.board.rows)
        self.board.wifi = {"ssid": "custom", "pass": "custom-pass"}
        self.board.users = [{"name": "admin", "role": 1, "pass": "custom-admin"}]
        self.board.session = {"user": "admin", "role": 1}
        req = Request({"confirm": "ERASE_ESP"}, True)
        req.route("POST")
        self.assertEqual(req.result[0], 200)
        self.assertEqual(lab.json.loads(req.result[2]), {"ok": 1, "erased": 1, "restart": 1})
        self.assertFalse(self.board.rows or self.board.charge_history or self.board.chart_buckets)
        self.assertFalse(any(self.board.chart.values()))
        self.assertIsNone(self.board.session)
        self.assertEqual(self.board.wifi, {"ssid": "changeover_userpannel", "pass": "123456789"})
        self.assertEqual(self.board.users[0]["pass"], "admin")
        self.assertEqual(before[0], self.board.charge_counts)
        self.assertGreaterEqual(self.board.board_uptime, before[1])
        self.assertEqual(before[2], self.board.latch)
if __name__ == "__main__":
    unittest.main()
