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
if __name__ == "__main__":
    unittest.main()
