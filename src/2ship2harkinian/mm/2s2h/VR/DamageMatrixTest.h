#pragma once
#include "DamageAuditCatalog.inc"
// Exercises the real native dispatch/application functions against every source
// table. Actors here are neutral fixtures: live enemy AI/hurtboxes are separate.
static void NativeDamageMatrixTest(PlayState* play) {
    std::ofstream log("native-damage-matrix.log");
    constexpr float multipliers[]={0,1,2,.5f,.25f,3,4,1,1,1,1,1,1,1,1,1};
    int cases=0,failures=0;
    for(auto& row:nativeDamageAuditRows) {
        int rowCases=0,rowFailures=0;
        for(unsigned bit=0;bit<32;++bit) for(int power:{1,2,3,4,8,16})
        for(int defense:{0,1,4}) for(bool hard:{false,true}) {
            Actor enemy{},source{};Collider attack{},target{};
            ColliderElement attackElement{},targetElement{};
            enemy.colChkInfo.damageTable=&row.table;
            target.actor=&enemy;attack.actor=&source;
            attackElement.atDmgInfo.dmgFlags=uint32_t(1)<<bit;
            attackElement.atDmgInfo.damage=power;
            target.acFlags=AC_HIT|(hard?AC_HARD:0);
            targetElement.acElemFlags=ACELEM_HIT;
            targetElement.acDmgInfo.defense=defense;
            targetElement.acHit=&attack;targetElement.acHitElem=&attackElement;
            const float scaled=power*multipliers[row.table.attack[bit]&15];
            const unsigned expectedEffect=row.table.attack[bit]>>4;
            u32 effect=999;
            const float actual=CollisionCheck_GetDamageAndEffectOnElementAC(&attack,&attackElement,&target,&targetElement,&effect);
            CollisionCheck_ApplyDamage(play,&play->colChkCtx,&target,&targetElement);
            const float defended=scaled<1?0:scaled-defense;
            const unsigned expectedDamage=(!hard||bit==29)&&defended>=1?unsigned(defended):0;
            const bool ok=std::fabs(actual-scaled)<.001f&&effect==expectedEffect&&
                enemy.colChkInfo.damage==expectedDamage&&enemy.colChkInfo.damageEffect==expectedEffect;
            ++cases;++rowCases;
            if(!ok){++failures;++rowFailures;log<<"FAIL "<<row.source<<' '<<row.name<<" bit="<<bit<<" power="<<power<<" defense="<<defense<<" hard="<<hard<<" damage="<<int(enemy.colChkInfo.damage)<<" expected="<<expectedDamage<<" effect="<<effect<<" expectedEffect="<<expectedEffect<<'\n';}
        }
        log<<"table="<<row.source<<':'<<row.name<<" cases="<<rowCases<<" failures="<<rowFailures<<'\n';
    }
    log<<"tables="<<ARRAY_COUNT(nativeDamageAuditRows)<<" cases="<<cases<<" failures="<<failures<<'\n'<<std::flush;
    Ship::Context::GetRawInstance()->GetWindow()->Close();
}
