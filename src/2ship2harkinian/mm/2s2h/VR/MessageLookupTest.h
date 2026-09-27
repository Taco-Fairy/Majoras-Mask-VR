#pragma once
#include "2s2h/CustomMessage/CustomMessage.h"
static void NativeMessageLookupTest(PlayState* play) {
    auto* font=&play->msgCtx.font;
    const auto savedStart=font->messageStart;
    const auto savedType=font->charBuf[font->unk_11D88][0];
    unsigned failures=0,cases=0;
    std::ofstream log("native-message-lookup.log");
    for(bool credits:{false,true}) {
        auto* table=credits?play->msgCtx.messageTableCredits:play->msgCtx.messageTableNES;
        std::vector<MessageTableEntry*> expected(65536,table);
        unsigned entries=0;
        for(auto* row=table;row->textId!=0xFFFF;++row) {
            if(++entries>65535){++failures;break;}
            if(expected[row->textId]==table)expected[row->textId]=row;
        }
        for(unsigned id=0;id<=65535;++id) {
            font->messageStart=0;
            if(credits)Message_FindCreditsMessage(play,id);else Message_FindMessageNES(play,id);
            const bool ok=(void*)font->messageStart==(void*)expected[id] &&
                (!credits||font->charBuf[font->unk_11D88][0]==expected[id]->typePos);
            ++cases;
            if(!ok){++failures;if(failures<20)log<<"FAIL credits="<<credits<<" id="<<id<<'\n';}
        }
        log<<"credits="<<credits<<" entries="<<entries<<'\n';
    }
    const auto first = CustomMessage::LoadVanillaMessageTableEntry(play->msgCtx.messageTableNES[0].textId);
    const auto missing = CustomMessage::LoadVanillaMessageTableEntry(0xFFFF);
    if (first.msg != missing.msg || first.textboxType != missing.textboxType || first.icon != missing.icon) {
        ++failures; log << "FAIL custom missing-ID fallback\n";
    }
    font->messageStart=savedStart;font->charBuf[font->unk_11D88][0]=savedType;
    log<<"cases="<<cases<<" failures="<<failures<<'\n'<<std::flush;
    Ship::Context::GetRawInstance()->GetWindow()->Close();
}
