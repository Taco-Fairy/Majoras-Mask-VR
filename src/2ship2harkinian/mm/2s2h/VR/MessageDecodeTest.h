#pragma once
#include "message_data_fmt_nes.h"
extern "C" void Message_Decode(PlayState*);
// Real message opening and first-page decoding. Quest branches and later pages
// remain separate lifecycle tests; this is not a visual/readability oracle.
static void NativeMessageDecodeTest(PlayState* play) {
    const auto saved=std::make_unique<MessageContext>(play->msgCtx);
    const auto savedSave=std::make_unique<SaveContext>(gSaveContext);
    gSaveContext.dungeonSceneSharedIndex=0;
    std::ofstream log("native-message-decode.log");
    const bool allPages=std::getenv("MMVR_MESSAGE_PAGES_TEST")!=nullptr;
    unsigned cases=0,failures=0,pages=0;
    for(bool credits:{false,true}) {
        auto* table=credits?saved->messageTableCredits:saved->messageTableNES;
        for(auto* row=table;row->textId!=0xFFFF;++row) {
            ++cases;log<<"begin id="<<row->textId<<" credits="<<credits<<" size="<<row->msgSize<<'\n'<<std::flush;
            if(!row->segment || row->msgSize>sizeof(play->msgCtx.font.msgBuf) || row->msgSize==0) {++failures;log<<"FAIL message resource bounds\n";continue;}
            play->msgCtx=*saved;
            Message_StartTextbox(play,row->textId,nullptr);
            unsigned pageCount=0;
            while(true) {
                Message_Decode(play); ++pages; ++pageCount;
                auto& ctx=play->msgCtx;
                const bool valid=ctx.msgBufPos<sizeof(ctx.font.msgBuf)&&ctx.decodedTextLen<ARRAY_COUNT(ctx.decodedBuffer.schar);
                if(!valid){++failures;log<<"FAIL decoded bounds id="<<row->textId<<'\n';break;}
                if(!allPages || credits)break;
                // Mirror native page advancement only. Fade/event/choice/continue
                // outcomes still require separate dialogue lifecycle scenarios.
                const int end=ctx.decodedBuffer.schar[ctx.decodedTextLen];
                const bool delayed=ctx.decodedTextLen>=2 &&
                    ctx.decodedBuffer.schar[ctx.decodedTextLen-2]==MESSAGE_BOX_BREAK_DELAYED;
                if(end==MESSAGE_BOX_BREAK || end==MESSAGE_BOX_BREAK2)++ctx.msgBufPos;
                else if(!delayed)break;
                if(pageCount>=128 || ctx.msgBufPos>=sizeof(ctx.font.msgBuf)) {
                    ++failures;log<<"FAIL page progression bounds id="<<row->textId<<'\n';break;
                }
            }
        }
    }
    play->msgCtx=*saved;gSaveContext=*savedSave;
    log<<"decodedPages="<<pages<<" allNativePages="<<allPages<<'\n';
    log<<"cases="<<cases<<" failures="<<failures<<'\n'<<std::flush;
    Ship::Context::GetRawInstance()->GetWindow()->Close();
}
