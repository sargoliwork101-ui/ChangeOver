/* Strict reset-command validation through the production HTTP handler. */
static void func__T_TestResetValidation(void)
{
    printf("\n== strict reset command validation ==\n");
    func__Simple_AuthLogout();func__Simple_LoginClear();func__Simple_AuthDefaults();
    func__T_ClearCookie();SIMPLE_WEB_SERVER_T__G__Server.simSetHeader("X-Admin-Token","");
    SIMPLE_WEB_SERVER_T__G__Server.on("/latch/reset",HTTP_POST,func__Simple_HttpLatchReset);
    func__T_ClearQueue();const uint32_t before=UINT32_T__G__SlotRequestCount;
    for(const char *body:{"n=5","n=5junk"})
    {
        SIMPLE_WEB_SERVER_T__G__Server.simCall("/latch/reset",HTTP_POST,body);
        func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus()==403 &&
            UINT32_T__G__SlotRequestCount==before,"reset validation never bypasses the administrator gate");
    }
    (void)func__Simple_AuthLogin("admin","admin");
    SIMPLE_WEB_SERVER_T__G__Server.simSetHeader("X-Admin-Token",CHAR__G__SessionToken);
    const char *invalid[]={"","n=","n=5junk","n=6junk","n=8junk","n=0","n=7","n=9",
        "n=55","n=05","n=-5","n=%2B5","n=%205","n=5%20","n=5.0","n=5e0",
        "n=4294967301","n=%DB%B5","n=%22%7D","n=%09"};
    for(const char *body:invalid)
    {
        SIMPLE_WEB_SERVER_T__G__Server.simCall("/latch/reset",HTTP_POST,body);
        func__T_Check(SIMPLE_WEB_SERVER_T__G__Server.simStatus()==400 &&
            SIMPLE_WEB_SERVER_T__G__Server.simBody()=="{\"ok\":0,\"e\":\"reset\",\"allowed\":[5,6,8]}" &&
            UINT32_T__G__SlotRequestCount==before && func__T_PendingCount()==0u,
            "malformed or unsupported reset returns valid error JSON and allowed commands without queueing");
    }
    const uint8_t modes[]={5u,6u,8u};
    const uint8_t ids[][3]={{SIMPLE_SLOT_EVENTS_ID,SIMPLE_SLOT_CYCLES_ID,SIMPLE_SLOT_LATCH_ID},
        {SIMPLE_SLOT_DEAD_MASK_ID,0u,0u},{SIMPLE_SLOT_WEAK_RESET_ID,0u,0u}};
    for(unsigned i=0;i<3u;i++)
    {
        func__T_ClearQueue();const unsigned expected=i==0u?3u:1u;
        std::string query="n="+std::to_string(modes[i]);
        SIMPLE_WEB_SERVER_T__G__Server.simCall("/latch/reset",HTTP_POST,query.c_str());
        bool same=SIMPLE_WEB_SERVER_T__G__Server.simStatus()==200 && func__T_PendingCount()==expected &&
            SIMPLE_WEB_SERVER_T__G__Server.simBody()==("{\"ok\":1,\"n\":"+std::to_string(modes[i])+"}");
        for(unsigned j=0;j<expected;j++)same=same && UINT8_T__G__SlotQueueId[j]==ids[i][j] && UINT32_T__G__SlotQueueValue[j]==0u;
        func__T_Check(same,"each permitted reset returns a numeric JSON value and preserves its slot IDs and ordering");
    }
    func__T_ClearQueue();func__Simple_AuthLogout();func__T_ClearCookie();
    SIMPLE_WEB_SERVER_T__G__Server.simSetHeader("X-Admin-Token","");
}
