// Included inside namespace Fast. All pools below belong to fresh interpreters;
// no GPU renderer, live map, shader cache, or gameplay resource is touched.
namespace {
bool CombinerProbeEqual(const ColorCombiner& a, const ColorCombiner& b) {
    return a.shader_id0==b.shader_id0 && a.shader_id1==b.shader_id1 &&
        std::equal(std::begin(a.usedTextures),std::end(a.usedTextures),std::begin(b.usedTextures)) &&
        std::equal(std::begin(a.prg),std::end(a.prg),std::begin(b.prg)) &&
        std::memcmp(a.shader_input_mapping,b.shader_input_mapping,sizeof(a.shader_input_mapping))==0;
}
}

void Interpreter::WriteColorCombinerCacheDiagnostics(const char* path) const {
    if(!mColorCombinerCacheDiagnosticsEnabled||!path)return;
    const char* session=std::getenv("MMVR_SESSION_TOKEN");
    const auto& c=mColorCombinerCacheDiagnostics;
    std::ofstream(path)<<"{\"session\":\""<<(session?session:"")<<"\",\"immediateHits\":"<<c.immediateHits
        <<",\"frontHits\":"<<c.frontHits<<",\"mapHits\":"<<c.mapHits<<",\"misses\":"<<c.misses
        <<",\"collisions\":"<<c.collisions<<",\"clears\":"<<c.clears
        <<",\"lookups\":"<<(c.immediateHits+c.frontHits+c.mapHits+c.misses)
        <<",\"entries\":"<<mColorCombinerPool.size()<<"}";
}

void Interpreter::VerifyColorCombinerCache() {
    const char* diagnostic=std::getenv("MMVR_NATIVE_TEST");
    if(!diagnostic||std::strcmp(diagnostic,"1")!=0)return;
    Interpreter probe,baseline;
    constexpr uint64_t cycle=uint64_t(G_CCMUX_TEXEL0)|(uint64_t(8)<<4)|
        (uint64_t(G_CCMUX_SHADE)<<8)|(uint64_t(7)<<13)|
        (uint64_t(G_ACMUX_TEXEL0)<<16)|(uint64_t(G_ACMUX_0)<<19)|
        (uint64_t(G_ACMUX_SHADE)<<22)|(uint64_t(G_ACMUX_0)<<25);
    const ColorCombinerKey base{cycle|(cycle<<28),0};
    auto* original=probe.LookupOrCreateColorCombiner(base);
    bool sameKey=original==&probe.mColorCombinerPool.at(base);
    const auto firstCounts=probe.mColorCombinerCacheDiagnostics;
    constexpr unsigned repeats=1000;
    for(unsigned i=0;i<repeats;++i)sameKey&=probe.LookupOrCreateColorCombiner(base)==original;
    sameKey&=probe.mColorCombinerPool.size()==1 &&
        probe.mColorCombinerCacheDiagnostics.immediateHits==firstCounts.immediateHits+repeats &&
        probe.mColorCombinerCacheDiagnostics.frontHits==firstCounts.frontHits &&
        probe.mColorCombinerCacheDiagnostics.misses==firstCounts.misses;

    // Alternate non-colliding keys, then repeat a front hit immediately: both
    // shortcuts must return the map's exact node, not a reconstructed combiner.
    ColorCombinerKey other=base;
    for(uint64_t id=1;id<4096;++id){
        other.options=id<<SHADER_ID_SHIFT;
        if(ColorCombinerFrontIndex(other)!=ColorCombinerFrontIndex(base))break;
    }
    auto* otherNode=probe.LookupOrCreateColorCombiner(other);
    const auto beforeAlternating=probe.mColorCombinerCacheDiagnostics;
    bool alternating=ColorCombinerFrontIndex(other)!=ColorCombinerFrontIndex(base)&&otherNode!=original;
    for(unsigned i=0;i<repeats;++i){
        alternating&=probe.LookupOrCreateColorCombiner(base)==original;
        alternating&=probe.LookupOrCreateColorCombiner(other)==otherNode;
    }
    alternating&=probe.mColorCombinerCacheDiagnostics.frontHits==beforeAlternating.frontHits+2*repeats &&
        probe.mColorCombinerCacheDiagnostics.mapHits==beforeAlternating.mapHits &&
        probe.mColorCombinerCacheDiagnostics.misses==beforeAlternating.misses;
    const auto immediateBefore=probe.mColorCombinerCacheDiagnostics.immediateHits;
    bool immediateAfterFront=probe.LookupOrCreateColorCombiner(other)==otherNode &&
        probe.mColorCombinerCacheDiagnostics.immediateHits==immediateBefore+1;

    // Exercise both full-width key fields, including high custom shader-ID
    // bits and all material options which affect generated shader identity.
    std::array<ColorCombinerKey,12> variants{};variants.fill(base);
    variants[0].combine_mode^=uint64_t(1)<<13;
    variants[1].combine_mode^=uint64_t(1)<<41;
    variants[2].options=SHADER_OPT(ALPHA)|SHADER_OPT(FOG);
    variants[3].options=SHADER_OPT(_2CYC);
    variants[4].options=SHADER_OPT(TEXEL0_MASK)|SHADER_OPT(TEXEL1_MASK);
    variants[5].options=SHADER_OPT(TEXEL0_BLEND)|SHADER_OPT(TEXEL1_BLEND);
    variants[6].options=SHADER_OPT(GRAYSCALE)|SHADER_OPT(PRIM_DEPTH);
    variants[7].options=UINT64_C(0x4000)<<SHADER_ID_SHIFT;
    variants[8].options=UINT64_C(0x8000)<<SHADER_ID_SHIFT;
    variants[9].options=SHADER_OPT(TEXTURE_EDGE)|SHADER_OPT(NOISE)|SHADER_OPT(ALPHA_THRESHOLD);
    variants[10].combine_mode^=UINT64_C(1)<<63;
    variants[11].options=UINT64_C(1)<<63;
    bool fullKey=true,generatedEquivalent=true;
    std::set<ColorCombiner*> variantNodes{original};
    for(const auto& key:variants){
        auto* actual=probe.LookupOrCreateColorCombiner(key);
        ColorCombiner expected{};GenerateCC(&expected,key);
        fullKey&=actual==&probe.mColorCombinerPool.at(key)&&variantNodes.insert(actual).second;
        generatedEquivalent&=CombinerProbeEqual(*actual,expected);
    }
    fullKey&=variantNodes.size()==variants.size()+1;

    // Deliberately locate a hash collision. The full key must reject the front
    // entry and map fallback must recover the original object in both orders.
    ColorCombinerKey collision=base;bool collisionFound=false;
    for(uint64_t id=1;id<4096;++id){
        collision.options=id<<SHADER_ID_SHIFT;
        if(ColorCombinerFrontIndex(collision)==ColorCombinerFrontIndex(base)){collisionFound=true;break;}
    }
    auto* collisionNode=probe.LookupOrCreateColorCombiner(collision);
    const auto beforeCollision=probe.mColorCombinerCacheDiagnostics;
    bool collisions=collisionFound&&collisionNode!=original;
    for(unsigned i=0;i<32;++i){
        collisions&=probe.LookupOrCreateColorCombiner(base)==original;
        collisions&=probe.LookupOrCreateColorCombiner(collision)==collisionNode;
    }
    collisions&=probe.mColorCombinerCacheDiagnostics.collisions==beforeCollision.collisions+64 &&
        probe.mColorCombinerCacheDiagnostics.mapHits==beforeCollision.mapHits+64 &&
        probe.mColorCombinerCacheDiagnostics.misses==beforeCollision.misses;

    // Grow the authoritative map substantially and compare every generated
    // field to the original lookup algorithm, including all cached addresses.
    std::vector<ColorCombinerKey> keys;keys.reserve(512);
    std::vector<ColorCombiner*> addresses;addresses.reserve(512);
    bool baselineEquivalent=true;
    for(uint64_t id=0;id<512;++id){
        ColorCombinerKey key=base;key.options=id<<SHADER_ID_SHIFT;
        if(id&1)key.options|=SHADER_OPT(_2CYC)|SHADER_OPT(ALPHA);
        if(id&2)key.options|=SHADER_OPT(FOG);
        keys.push_back(key);
        auto* actual=probe.LookupOrCreateColorCombiner(key);addresses.push_back(actual);
        auto* expected=baseline.LookupOrCreateColorCombinerImpl<false>(key);
        baselineEquivalent&=CombinerProbeEqual(*actual,*expected);
    }
    bool poolGrowth=true;
    for(size_t i=keys.size();i-- >0;){
        auto* actual=probe.LookupOrCreateColorCombiner(keys[i]);
        poolGrowth&=actual==addresses[i]&&actual==&probe.mColorCombinerPool.at(keys[i]);
        baselineEquivalent&=CombinerProbeEqual(*actual,*baseline.LookupOrCreateColorCombinerImpl<false>(keys[i]));
    }

    probe.ColorCombinerCacheClear();
    bool clearReload=probe.mColorCombinerPool.empty()&&probe.mPrevCombiner==probe.mColorCombinerPool.end();
    for(const auto& entry:probe.mColorCombinerFront)clearReload&=!entry.valid;
    const auto beforeReload=probe.mColorCombinerCacheDiagnostics;
    auto* reloaded=probe.LookupOrCreateColorCombiner(base);
    ColorCombiner regenerated{};GenerateCC(&regenerated,base);
    clearReload&=reloaded==&probe.mColorCombinerPool.at(base)&&CombinerProbeEqual(*reloaded,regenerated)&&
        probe.mColorCombinerCacheDiagnostics.misses==beforeReload.misses+1&&
        probe.mColorCombinerCacheDiagnostics.frontHits==beforeReload.frontHits&&
        probe.mColorCombinerCacheDiagnostics.immediateHits==beforeReload.immediateHits;
    // Deliberately do not compare old pointer values after clear: the allocator
    // may legitimately reuse their addresses for these new objects.
    probe.ColorCombinerCacheClear();probe.ColorCombinerCacheClear();
    clearReload&=probe.mColorCombinerPool.empty();
    for(const auto& entry:probe.mColorCombinerFront)clearReload&=!entry.valid;
    clearReload&=CombinerProbeEqual(*probe.LookupOrCreateColorCombiner(collision),
                                   *baseline.LookupOrCreateColorCombinerImpl<false>(collision));

    // Counters are opt-in diagnostics, not writes on the ordinary rendering path.
    probe.mColorCombinerCacheDiagnosticsEnabled=false;
    const auto gatedBefore=probe.mColorCombinerCacheDiagnostics;
    probe.LookupOrCreateColorCombiner(base);probe.LookupOrCreateColorCombiner(base);
    probe.ColorCombinerCacheClear();
    const auto& gatedAfter=probe.mColorCombinerCacheDiagnostics;
    const bool diagnosticsGated=gatedBefore.immediateHits==gatedAfter.immediateHits&&
        gatedBefore.frontHits==gatedAfter.frontHits&&gatedBefore.mapHits==gatedAfter.mapHits&&
        gatedBefore.misses==gatedAfter.misses&&gatedBefore.collisions==gatedAfter.collisions&&
        gatedBefore.clears==gatedAfter.clears;

    // Microbenchmark a disclosed repeated 32-material working set in a 512-
    // material pool. It measures lookup work only, not gameplay FPS. Counters
    // are off for both paths; alternating run order reduces thermal/order bias.
    Interpreter fastBench,baselineBench;
    fastBench.mColorCombinerCacheDiagnosticsEnabled=false;
    baselineBench.mColorCombinerCacheDiagnosticsEnabled=false;
    std::array<bool,COLOR_COMBINER_FRONT_SIZE> selectedBuckets{};
    std::vector<ColorCombinerKey> hotKeys;
    for(const auto& key:keys){
        fastBench.LookupOrCreateColorCombiner(key);
        baselineBench.LookupOrCreateColorCombinerImpl<false>(key);
        const auto index=ColorCombinerFrontIndex(key);
        if(hotKeys.size()<32&&!selectedBuckets[index]){selectedBuckets[index]=true;hotKeys.push_back(key);}
    }
    for(const auto& key:hotKeys){fastBench.LookupOrCreateColorCombiner(key);baselineBench.LookupOrCreateColorCombinerImpl<false>(key);}
    constexpr unsigned iterations=100000,rounds=5;
    std::array<double,rounds> baselineNs{},fastNs{};
    bool benchmarkValid=hotKeys.size()==32;uint64_t baselineChecksum=0,fastChecksum=0;
    auto measure=[&]<bool Fast>(Interpreter& owner,uint64_t& result){
        uint64_t checksum=0;
        const auto begin=std::chrono::steady_clock::now();
        for(unsigned i=0;i<iterations;++i){
            auto* value=owner.LookupOrCreateColorCombinerImpl<Fast>(hotKeys[i%hotKeys.size()]);
            checksum+=value->shader_id0^(value->shader_id1+uint64_t(i));
        }
        const auto end=std::chrono::steady_clock::now();
        result=checksum;
        return std::chrono::duration<double,std::nano>(end-begin).count()/iterations;
    };
    for(unsigned round=0;round<rounds;++round){
        if(round&1){
            fastNs[round]=measure.template operator()<true>(fastBench,fastChecksum);
            baselineNs[round]=measure.template operator()<false>(baselineBench,baselineChecksum);
        }else{
            baselineNs[round]=measure.template operator()<false>(baselineBench,baselineChecksum);
            fastNs[round]=measure.template operator()<true>(fastBench,fastChecksum);
        }
        benchmarkValid&=baselineNs[round]>0&&fastNs[round]>0&&baselineChecksum==fastChecksum;
    }
    std::sort(baselineNs.begin(),baselineNs.end());std::sort(fastNs.begin(),fastNs.end());
    const bool isolated=!probe.mRapi&&!baseline.mRapi&&!fastBench.mRapi&&!baselineBench.mRapi&&
        probe.mBufVboLen==0&&baseline.mBufVboLen==0&&fastBench.mBufVboLen==0&&baselineBench.mBufVboLen==0;
    const bool passed=sameKey&&alternating&&immediateAfterFront&&fullKey&&collisions&&generatedEquivalent&&
        baselineEquivalent&&poolGrowth&&clearReload&&diagnosticsGated&&isolated&&benchmarkValid;
    const char* session=std::getenv("MMVR_SESSION_TOKEN");const auto& c=probe.mColorCombinerCacheDiagnostics;
    std::ofstream report("native-color-combiner-cache.json");
    report<<"{\"session\":\""<<(session?session:"")<<"\",\"passed\":"<<passed<<",\"repeats\":"<<repeats
        <<",\"sameKey\":"<<sameKey<<",\"alternating\":"<<alternating<<",\"immediateAfterFront\":"<<immediateAfterFront
        <<",\"fullKey\":"<<fullKey<<",\"collisions\":"<<collisions<<",\"generatedEquivalent\":"<<generatedEquivalent
        <<",\"baselineEquivalent\":"<<baselineEquivalent<<",\"poolGrowth\":"<<poolGrowth<<",\"clearReload\":"<<clearReload
        <<",\"diagnosticsGated\":"<<diagnosticsGated<<",\"isolated\":"<<isolated<<",\"benchmarkValid\":"<<benchmarkValid
        <<",\"immediateHits\":"<<c.immediateHits<<",\"frontHits\":"<<c.frontHits<<",\"mapHits\":"<<c.mapHits
        <<",\"misses\":"<<c.misses<<",\"collisionCount\":"<<c.collisions<<",\"clears\":"<<c.clears
        <<",\"benchmarkIterations\":"<<iterations<<",\"benchmarkRounds\":"<<rounds<<",\"benchmarkHotKeys\":"<<hotKeys.size()
        <<",\"benchmarkPoolSize\":"<<keys.size()<<",\"baselineNsPerLookup\":"<<baselineNs[rounds/2]
        <<",\"fastNsPerLookup\":"<<fastNs[rounds/2]<<",\"benchmarkChecksum\":"<<fastChecksum<<"}";
    report.close();
    if(!report)throw std::runtime_error("Could not write native color-combiner-cache report");
    if(!passed)throw std::runtime_error("Native color-combiner front-cache regression failed");
}
