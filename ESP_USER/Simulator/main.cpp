/* main.cpp - اجرای واقعی برنامهٔ پنل ساده روی PC
   فقط فارسی - مثل بقیهٔ این پوشه

   این فایل خودِ `../ESP_USER.ino` را include می‌کند: نه کپی، نه
   بازنویسی. پس `setup()` و `loop()` که در مرورگر کار می‌کنند همان‌هایی هستند
   که روی ESP8266 فلش می‌شوند و پشت آن‌ها:
   - `stub_web_server.h` یک وب‌سرور واقعی روی سوکت TCP (پورت پیش‌فرض ۳۰۰۰)،
   - `sim_board.h` یک STM32 شبیه‌سازی‌شده که قاب‌های `TLM_LIVE` و `DIAG_DATA`
     را دقیقاً با چیدمان فرم‌ور می‌فرستد و `TIME_SYNC`/`DIAG_REQ` را می‌خواند.

   دو مسیر اضافه (فقط سیمولاتور، نه روی برد):
   - `POST /sim?...` مقادیر اسلایدرهای تب «سیمولاتور» را در برد می‌نشاند.
   - صفحهٔ `/` پس از ساخته‌شدن توسط خود برنامه، همان shim تب چهارم را که
     `make_simple_preview.py` ساخته در `</head>` تزریق می‌کند؛ بدنهٔ پنل
     دست‌نخورده است.

   اجرا:  ESP_USER/Simulator/run_sim.sh
*/
#include "stub_arduino.h"
#include "stub_web_server.h"

#include "../ESP_USER.ino"   /* خود برنامه، دست‌نخورده */

#include "sim_board.h"

#include <fstream>
#include <sstream>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ==================== نمونه‌های سراسری استاب‌ها ==================== */
SimSerialLink Serial;
SimWiFi WiFi;
SimEsp ESP;
SimBoardState SIM_BOARD_STATE_T__G__Board;
static std::string STD_STRING_T__G__FsRoot = "/tmp/changeover_simple_fs";

/* ==================== فایل‌سیستم شبیه‌سازی‌شده ==================== */
SimLittleFS LittleFS;

static std::string func__Sim_FsPath(const char *char__ptr_path)
{
    std::string string__full = STD_STRING_T__G__FsRoot;
    if ((char__ptr_path != NULL) && (char__ptr_path[0] != '\0'))
    {
        if (char__ptr_path[0] != '/') { string__full += "/"; }
        string__full += char__ptr_path;
    }
    return string__full;
}

bool SimLittleFS::begin(bool bool__formatOnFail)
{
    (void)bool__formatOnFail;

    const char *char__ptr_overrideRoot = getenv("SIMPLE_SIM_FS");
    if ((char__ptr_overrideRoot != NULL) && (char__ptr_overrideRoot[0] != '\0'))
    {
        STD_STRING_T__G__FsRoot = char__ptr_overrideRoot;
    }

    struct stat struct_stat__info;
    if ((stat(STD_STRING_T__G__FsRoot.c_str(), &struct_stat__info) != 0) &&
        (mkdir(STD_STRING_T__G__FsRoot.c_str(), 0755) != 0))
    {
        printf("[sim] پوشهٔ فایل‌سیستم ساخته نشد: %s\n", STD_STRING_T__G__FsRoot.c_str());
        return false;
    }
    printf("[sim] فایل‌سیستم شبیه‌سازی‌شده: %s\n", STD_STRING_T__G__FsRoot.c_str());
    return true;
}

bool SimLittleFS::format(void)
{
    DIR *dir__handle = opendir(STD_STRING_T__G__FsRoot.c_str());
    if (dir__handle != NULL)
    {
        struct dirent *entry__item;
        while ((entry__item = readdir(dir__handle)) != NULL)
        {
            if (entry__item->d_name[0] == '.') { continue; }
            std::string string__victim = STD_STRING_T__G__FsRoot + "/" + std::string(entry__item->d_name);
            (void)::remove(string__victim.c_str());
        }
        closedir(dir__handle);
    }
    return true;
}

bool SimLittleFS::info(FSInfo &fsInfo__out)
{
    size_t size_t__used = 0u;
    DIR *dir__handle = opendir(STD_STRING_T__G__FsRoot.c_str());
    if (dir__handle != NULL)
    {
        struct dirent *entry__item;
        while ((entry__item = readdir(dir__handle)) != NULL)
        {
            if (entry__item->d_name[0] == '.') { continue; }
            struct stat struct_stat__st;
            std::string string__p = STD_STRING_T__G__FsRoot + "/" + std::string(entry__item->d_name);
            if (stat(string__p.c_str(), &struct_stat__st) == 0)
            {
                size_t__used += (size_t)struct_stat__st.st_size;
            }
        }
        closedir(dir__handle);
    }
    fsInfo__out.totalBytes = 2097152u;
    fsInfo__out.usedBytes = size_t__used;
    return true;
}

File SimLittleFS::open(const char *char__ptr_path, const char *char__ptr_mode)
{
    return File(func__Sim_FsPath(char__ptr_path), char__ptr_mode);
}

bool SimLittleFS::remove(const char *char__ptr_path)
{
    return ::remove(func__Sim_FsPath(char__ptr_path).c_str()) == 0;
}

bool SimLittleFS::exists(const char *char__ptr_path)
{
    struct stat struct_stat__info;
    return stat(func__Sim_FsPath(char__ptr_path).c_str(), &struct_stat__info) == 0;
}

/* [EN] Same door the real LittleFS has: the panel walks the folder to build its
        day list, so the simulator hands it the file names.
   [FA] همان دری که LittleFS واقعی دارد: پنل برای ساختن فهرست روزها پوشه را
        می‌گردد، پس شبیه‌ساز نام فایل‌ها را به آن می‌دهد. */
SimDir SimLittleFS::openDir(const char *char__ptr_path)
{
    SimDir dir__result;
    std::string string__full = func__Sim_FsPath(char__ptr_path);
    DIR *dir__handle = opendir(string__full.c_str());

    if (dir__handle == NULL)
    {
        return dir__result;
    }
    {
        struct dirent *entry__item;

        while ((entry__item = readdir(dir__handle)) != NULL)
        {
            if (entry__item->d_name[0] == '.')
            {
                continue;
            }
            struct stat struct_stat__st;
            std::string string__p = string__full + "/" + std::string(entry__item->d_name);
            size_t size_t__sz = (stat(string__p.c_str(), &struct_stat__st) == 0)
                ? (size_t)struct_stat__st.st_size : 0u;
            /* شبیه‌سازی دقیق ESP8266 واقعی: dir.fileName() نام خام بدون «/» برمی‌گرداند */
            dir__result.names.push_back(std::string(entry__item->d_name));
            dir__result.sizes.push_back(size_t__sz);
        }
    }
    closedir(dir__handle);
    return dir__result;
}

/* ==================== shim تب سیمولاتور برای صفحهٔ "/" ==================== */
static std::string STD_STRING_T__G__PanelShim;
static bool BOOL__G__ShimWarned = false;

/* مسیر فایل تولیدشده از محل خود همین فایل مشتق می‌شود تا اجرا از هر پوشه‌ای
   کار کند */
static std::string func__Sim_PreviewPath(void)
{
    const char *char__ptr_overridePath = getenv("SIMPLE_SIM_PREVIEW");
    if ((char__ptr_overridePath != NULL) && (char__ptr_overridePath[0] != '\0'))
    {
        return char__ptr_overridePath;
    }

    std::string string__here = __FILE__;
    size_t size_t__cut = string__here.rfind('/');
    if (size_t__cut == std::string::npos) { return "simple_preview.html"; }

    std::string string__simulatorDir = string__here.substr(0u, size_t__cut);
    size_t size_t__parent = string__simulatorDir.rfind('/');
    std::string string__panelDir =
        (size_t__parent == std::string::npos) ? string__simulatorDir
                                              : string__simulatorDir.substr(0u, size_t__parent);
    return string__panelDir + "/simple_preview.html";
}

static void func__Sim_LoadShim(void)
{
    const std::string string__path = func__Sim_PreviewPath();
    std::ifstream ifstream__input(string__path.c_str(), std::ios::binary);
    if (!ifstream__input)
    {
        printf("[sim] هشدار: %s پیدا نشد؛ صفحه بدون تب سیمولاتور سرو می‌شود.\n", string__path.c_str());
        printf("[sim] برای ساختش: python3 ESP_USER/make_simple_preview.py\n");
        return;
    }

    std::ostringstream ostringstream__buffer;
    ostringstream__buffer << ifstream__input.rdbuf();
    const std::string string__page = ostringstream__buffer.str();

    const std::string string__beginMarker = "<!-- SIM_PREVIEW_SHIM_BEGIN";
    const size_t size_t__shimStart = string__page.find(string__beginMarker);
    const size_t size_t__headEnd =
        string__page.find("</head>", (size_t__shimStart == std::string::npos) ? 0u : size_t__shimStart);

    if ((size_t__shimStart == std::string::npos) || (size_t__headEnd == std::string::npos) ||
        (size_t__headEnd <= size_t__shimStart))
    {
        printf("[sim] هشدار: shim در %s پیدا نشد؛ صفحه بدون تب سیمولاتور سرو می‌شود.\n",
               string__path.c_str());
        return;
    }

    STD_STRING_T__G__PanelShim = string__page.substr(size_t__shimStart, size_t__headEnd - size_t__shimStart);
    printf("[sim] shim تب سیمولاتور از %s خوانده شد (%lu بایت)\n",
           string__path.c_str(), (unsigned long)STD_STRING_T__G__PanelShim.size());
}

/* پاسخ "/" که خود برنامه ساخت، پیش از فرستادن شبیه تب سیمولاتور می‌شود */
static bool func__Sim_InjectShim(const char *char__ptr_uri, std::string &string__body)
{
    if ((char__ptr_uri == NULL) || (strcmp(char__ptr_uri, "/") != 0) || STD_STRING_T__G__PanelShim.empty())
    {
        return false;
    }

    const size_t size_t__headEnd = string__body.find("</head>");
    if (size_t__headEnd == std::string::npos)
    {
        if (!BOOL__G__ShimWarned)
        {
            BOOL__G__ShimWarned = true;
            printf("[sim] هشدار: در بدنهٔ صفحهٔ / تگ </head> نبود؛ تزریق انجام نشد.\n");
        }
        return false;
    }

    string__body.insert(size_t__headEnd, STD_STRING_T__G__PanelShim);
    return true;
}

/* ==================== مسیر /sim: فقط سیمولاتور ==================== */
static void func__Sim_HttpSim(void)
{
    func__Sim_ApplyRequest(SIMPLE_WEB_SERVER_T__G__Server);

    char char__answer[256];
    (void)snprintf(char__answer, sizeof(char__answer),
                   "{\"ok\":1,\"vin\":%lu,\"vpack\":%lu,\"vlow\":%lu,\"vhigh\":%lu,"
                   "\"s1\":%lu,\"s2\":%lu,\"fault\":%lu,\"link\":%u}",
                   (unsigned long)SIM_BOARD_STATE_T__G__Board.vInMv,
                   (unsigned long)SIM_BOARD_STATE_T__G__Board.vPackMv,
                   (unsigned long)SIM_BOARD_STATE_T__G__Board.vLowMv,
                   (unsigned long)SIM_BOARD_STATE_T__G__Board.vHighMv,
                   (unsigned long)SIM_BOARD_STATE_T__G__Board.state1,
                   (unsigned long)SIM_BOARD_STATE_T__G__Board.state2,
                   (unsigned long)SIM_BOARD_STATE_T__G__Board.faultMask,
                   SIM_BOARD_STATE_T__G__Board.linkUp ? 1u : 0u);
    SIMPLE_WEB_SERVER_T__G__Server.send(200, "application/json", char__answer);
}

/* ==================== حلقهٔ سیمولاتور ====================
   اگر SIMPLE_SIM_LINK_DOWN=1 باشد، برد در شروع بدون لینک بالا می‌آید تا
   مسیر «بازگردانی از فایل» (ساعت/پارامترها/نمودارها) قابل آزمون باشد. */
static void func__Sim_ApplyBootFlags(void)
{
    const char *char__ptr_linkDown = getenv("SIMPLE_SIM_LINK_DOWN");
    if (char__ptr_linkDown != NULL && char__ptr_linkDown[0] == '1')
    {
        SIM_BOARD_STATE_T__G__Board.linkUp = false;
    }
}

static void func__Sim_PrintBanner(void)
{
    const char *char__ptr_portText = getenv("SIMPLE_SIM_PORT");

    printf("\n");
    printf("================ سیمولاتور پنل سادهٔ کاربر ================\n");
    printf(" برد   : STM32 شبیه‌سازی‌شده (پشت همان کد ESP که فلش می‌شود)\n");
    printf(" پنل   : http://localhost:%s   (در سندباکس: پیش‌نمایش همین پورت)\n",
           (char__ptr_portText != NULL) ? char__ptr_portText : "3000");
    printf(" ورود  : admin / admin   (نقش ساده: viewer / viewer)\n");
    printf(" کنترل: تب چهارم «سیمولاتور» در خود پنل\n");
    printf(" خروج  : Ctrl+C\n");
    printf("==========================================================\n\n");
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);

    func__Sim_LoadShim();
    func__Sim_ApplyBootFlags();
    func__Sim_PrintBanner();

    for (;;)
    {
        try
        {
            setup();   /* خود برنامه */
            SIMPLE_WEB_SERVER_T__G__Server.on("/sim", HTTP_ANY, func__Sim_HttpSim);
            SIMPLE_WEB_SERVER_T__G__Server.setResponseRewriter(func__Sim_InjectShim);

            for (;;)
            {
                func__Sim_Tick((uint32_t)millis());
                loop();   /* خود برنامه */
                usleep(500);
            }
        }
        catch (const SimRestartRequest &)
        {
            /* روی برد واقعی ESP.restart حافظهٔ RAM را پاک می‌کند. اگر اینجا
               فقط setup دوباره صدا زده شود، متغیرهای سراسری برنامه (مثل
               پرچم ریبوت وای‌فای) پاک نمی‌شوند و «ریست» بی‌اثر می‌ماند.
               پس مثل خود برد، برنامه از نو اجرا می‌شود: حافظه تازه، و فقط
               فایل‌سیستم روی دیسک به‌جامی‌ماند. */
            char char__exePath[512];
            ssize_t ssize_t__len = readlink("/proc/self/exe", char__exePath, sizeof(char__exePath) - 1u);
            printf("[sim] ریستارت: اجرای دوبارهٔ برنامه با حافظهٔ تازه\n");
            fflush(stdout);
            if (ssize_t__len > 0)
            {
                char__exePath[ssize_t__len] = '\0';
                execv(char__exePath, argv);
            }
            if ((argc > 0) && (argv[0] != NULL)) { execv(argv[0], argv); }
            return 0;   /* اگر اجرای دوباره ممکن نبود، بیرون بیا */
        }
    }
    return 0;
}
