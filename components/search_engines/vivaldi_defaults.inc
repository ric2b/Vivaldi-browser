// Copyright 2021 Vivaldi Technologies AS. All rights reserved.

// Search engines by locale. Every entry here MUST feature the default and
// private default search engines in first and second position and they
// must be repeated in the remainder of the list. Search engines will be ordered
// based on the order of these lists, excluding the first two entries.

constexpr EngineAndTier engines_default[] = {
    {SearchEngineTier::kTopEngines, &duckduckgo},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia},
    {SearchEngineTier::kTopEngines, &google},
};

constexpr EngineAndTier engines_be_BY[] = {
    {SearchEngineTier::kTopEngines, &yandex_by},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &yandex_by},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &ozon},
    {SearchEngineTier::kTopEngines, &wikipedia_be},
    {SearchEngineTier::kTopEngines, &ebay},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia_by},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wolfram},
};
constexpr EngineAndTier engines_bg_BG[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_bg},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_cs_CZ[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_cs},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_da_DK[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_dk},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_da},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_de_DE[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_de},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia_de},
    {SearchEngineTier::kTopEngines, &qwant_de},
    {SearchEngineTier::kTopEngines, &you},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_de},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_de_AT[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_at},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &wikipedia_de},
};
constexpr EngineAndTier engines_de_CH[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_ch},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_de},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_el_GR[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_el},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_en_CA[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_ca},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia_ca},
    {SearchEngineTier::kTopEngines, &qwant_ca},
    {SearchEngineTier::kTopEngines, &you},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &wikipedia},
};
constexpr EngineAndTier engines_en_GB[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_uk},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia_gb},
    {SearchEngineTier::kTopEngines, &qwant_gb},
    {SearchEngineTier::kTopEngines, &you},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_en_IE[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_ie},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &wikipedia},
};
constexpr EngineAndTier engines_en_IN[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_in},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &wikipedia},
};
constexpr EngineAndTier engines_en_PH[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &yahoo_ph},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &wikipedia},
};
constexpr EngineAndTier engines_en_SG[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &yahoo_sg},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &wikipedia},
};
constexpr EngineAndTier engines_en_US[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &duckduckgo},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_us},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yelp},
    {SearchEngineTier::kTopEngines, &ecosia_us},
    {SearchEngineTier::kTopEngines, &qwant_us},
    {SearchEngineTier::kTopEngines, &you},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_es_ES[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_es},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &qwant_es},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_es},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_es_AR[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &yahoo_ar},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &wikipedia_es},
};
constexpr EngineAndTier engines_es_CL[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &yahoo_cl},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &wikipedia_es},
};
constexpr EngineAndTier engines_es_CO[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &yahoo_co},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &wikipedia_es},
};
constexpr EngineAndTier engines_es_PE[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &yahoo_pe},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &wikipedia_es},
};
constexpr EngineAndTier engines_es_MX[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &yahoo_mx},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &wikipedia_es},
};
constexpr EngineAndTier engines_es_VE[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &yahoo_ve},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &wikipedia_es},
};
constexpr EngineAndTier engines_fi_FI[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_fi},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_fi},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_fr_FR[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_fr},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia_fr},
    {SearchEngineTier::kTopEngines, &qwant_fr},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_fr},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_hr_HR[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_hr},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_hu_HU[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_hu},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_id_ID[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_id},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_id},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_it_IT[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_it},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &qwant_it},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_it},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_ja_JP[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &duckduckgo},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_jp},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_ko_KR[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_ko},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_lt_LT[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_lt},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_nb_NO[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_no},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_no},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_nn_NO[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_no},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_nn},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_pl_PL[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_pl},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_pt_BR[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_br},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_pt},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_pt_PT[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_pt},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_ro_RO[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_ro},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_ru_RU[] = {
    {SearchEngineTier::kTopEngines, &yandex_ru},  // default
    {SearchEngineTier::kTopEngines, &duckduckgo},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &yandex_ru},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &wikipedia_ru},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &mail_ru},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wolfram},
};
constexpr EngineAndTier engines_sv_SE[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_se},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_se},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_sk_SK[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_sk},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_sr_RS[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_sr},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_th_TH[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &yahoo_th},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &wikipedia_th},
};
constexpr EngineAndTier engines_tr_TR[] = {
    {SearchEngineTier::kTopEngines, &duckduckgo},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yandex_tr},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &amazon},
    {SearchEngineTier::kTopEngines, &wikipedia_tr},
    {SearchEngineTier::kTopEngines, &ebay},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wolfram},
};
constexpr EngineAndTier engines_uk_UA[] = {
    {SearchEngineTier::kTopEngines, &duckduckgo},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &wikipedia_uk},
    {SearchEngineTier::kTopEngines, &ebay},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wolfram},
};
constexpr EngineAndTier engines_vi_VN[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_vn},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_vi},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_zh_CN[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &yahoo},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_zh},
    {SearchEngineTier::kTopEngines, &google},
};
constexpr EngineAndTier engines_zn_HK[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &yahoo_hk},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &wikipedia_zh},
};
constexpr EngineAndTier engines_zh_TW[] = {
    {SearchEngineTier::kTopEngines, &bing},  // default
    {SearchEngineTier::kTopEngines, &startpage_com},  // defaultPrivate
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_tw},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &startpage_com},
    {SearchEngineTier::kTopEngines, &wikipedia_zh},
    {SearchEngineTier::kTopEngines, &google},
};

constexpr EngineAndTier engines_unittests[] = {
    {SearchEngineTier::kTopEngines, &google_unittests},
    {SearchEngineTier::kTopEngines, &google_unittests},
    {SearchEngineTier::kTopEngines, &google_unittests},
    {SearchEngineTier::kTopEngines, &bing_unittests},
    {SearchEngineTier::kTopEngines, &yahoo_unittests},
};

// Search engines by country
const PrepopulatedPerLanguage engines_BY[] = {
    {"be", engines_be_BY, std::size(engines_be_BY)},
};
const PrepopulatedPerLanguage engines_BG[] = {
    {"bg", engines_bg_BG, std::size(engines_bg_BG)},
};
const PrepopulatedPerLanguage engines_CZ[] = {
    {"cs", engines_cs_CZ, std::size(engines_cs_CZ)},
};
const PrepopulatedPerLanguage engines_DK[] = {
    {"da", engines_da_DK, std::size(engines_da_DK)},
};
const PrepopulatedPerLanguage engines_DE[] = {
    {"de", engines_de_DE, std::size(engines_de_DE)},
};
const PrepopulatedPerLanguage engines_AT[] = {
    {"de", engines_de_AT, std::size(engines_de_AT)},
};
const PrepopulatedPerLanguage engines_CH[] = {
    {"de", engines_de_CH, std::size(engines_de_CH)},
};
const PrepopulatedPerLanguage engines_GR[] = {
    {"el", engines_el_GR, std::size(engines_el_GR)},
};
const PrepopulatedPerLanguage engines_CA[] = {
    {"en", engines_en_CA, std::size(engines_en_CA)},
};
const PrepopulatedPerLanguage engines_GB[] = {
    {"en", engines_en_GB, std::size(engines_en_GB)},
};
const PrepopulatedPerLanguage engines_IE[] = {
    {"en", engines_en_IE, std::size(engines_en_IE)},
};
const PrepopulatedPerLanguage engines_IN[] = {
    {"en", engines_en_IN, std::size(engines_en_IN)},
};
const PrepopulatedPerLanguage engines_PH[] = {
    {"en", engines_en_PH, std::size(engines_en_PH)},
};
const PrepopulatedPerLanguage engines_SG[] = {
    {"en", engines_en_SG, std::size(engines_en_SG)},
};
const PrepopulatedPerLanguage engines_US[] = {
    {"en", engines_en_US, std::size(engines_en_US)},
};
const PrepopulatedPerLanguage engines_ES[] = {
    {"es", engines_es_ES, std::size(engines_es_ES)},
};
const PrepopulatedPerLanguage engines_AR[] = {
    {"es", engines_es_AR, std::size(engines_es_AR)},
};
const PrepopulatedPerLanguage engines_CL[] = {
    {"es", engines_es_CL, std::size(engines_es_CL)},
};
const PrepopulatedPerLanguage engines_CO[] = {
    {"es", engines_es_CO, std::size(engines_es_CO)},
};
const PrepopulatedPerLanguage engines_PE[] = {
    {"es", engines_es_PE, std::size(engines_es_PE)},
};
const PrepopulatedPerLanguage engines_MX[] = {
    {"es", engines_es_MX, std::size(engines_es_MX)},
};
const PrepopulatedPerLanguage engines_VE[] = {
    {"es", engines_es_VE, std::size(engines_es_VE)},
};
const PrepopulatedPerLanguage engines_FI[] = {
    {"fi", engines_fi_FI, std::size(engines_fi_FI)},
};
const PrepopulatedPerLanguage engines_FR[] = {
    {"fr", engines_fr_FR, std::size(engines_fr_FR)},
};
const PrepopulatedPerLanguage engines_HR[] = {
    {"hr", engines_hr_HR, std::size(engines_hr_HR)},
};
const PrepopulatedPerLanguage engines_HU[] = {
    {"hu", engines_hu_HU, std::size(engines_hu_HU)},
};
const PrepopulatedPerLanguage engines_ID[] = {
    {"id", engines_id_ID, std::size(engines_id_ID)},
};
const PrepopulatedPerLanguage engines_IT[] = {
    {"it", engines_it_IT, std::size(engines_it_IT)},
};
const PrepopulatedPerLanguage engines_JP[] = {
    {"ja", engines_ja_JP, std::size(engines_ja_JP)},
};
const PrepopulatedPerLanguage engines_KR[] = {
    {"ko", engines_ko_KR, std::size(engines_ko_KR)},
};
const PrepopulatedPerLanguage engines_LT[] = {
    {"lt", engines_lt_LT, std::size(engines_lt_LT)},
};
const PrepopulatedPerLanguage engines_NO[] = {
    {"nb", engines_nb_NO, std::size(engines_nb_NO)},
    {"nn", engines_nn_NO, std::size(engines_nn_NO)},
};
const PrepopulatedPerLanguage engines_PL[] = {
    {"pl", engines_pl_PL, std::size(engines_pl_PL)},
};
const PrepopulatedPerLanguage engines_BR[] = {
    {"pt", engines_pt_BR, std::size(engines_pt_BR)},
};
const PrepopulatedPerLanguage engines_PT[] = {
    {"pt", engines_pt_PT, std::size(engines_pt_PT)},
};
const PrepopulatedPerLanguage engines_RO[] = {
    {"ro", engines_ro_RO, std::size(engines_ro_RO)},
};
const PrepopulatedPerLanguage engines_RU[] = {
    {"ru", engines_ru_RU, std::size(engines_ru_RU)},
};
const PrepopulatedPerLanguage engines_SE[] = {
    {"sv", engines_sv_SE, std::size(engines_sv_SE)},
};
const PrepopulatedPerLanguage engines_SK[] = {
    {"sk", engines_sk_SK, std::size(engines_sk_SK)},
};
const PrepopulatedPerLanguage engines_RS[] = {
    {"sr", engines_sr_RS, std::size(engines_sr_RS)},
};
const PrepopulatedPerLanguage engines_TH[] = {
    {"th", engines_th_TH, std::size(engines_th_TH)},
};
const PrepopulatedPerLanguage engines_TR[] = {
    {"tr", engines_tr_TR, std::size(engines_tr_TR)},
};
const PrepopulatedPerLanguage engines_UA[] = {
    {"uk", engines_uk_UA, std::size(engines_uk_UA)},
};
const PrepopulatedPerLanguage engines_VN[] = {
    {"vi", engines_vi_VN, std::size(engines_vi_VN)},
};
const PrepopulatedPerLanguage engines_CN[] = {
    {"zh", engines_zh_CN, std::size(engines_zh_CN)},
};
const PrepopulatedPerLanguage engines_HK[] = {
    {"zn", engines_zn_HK, std::size(engines_zn_HK)},
};
const PrepopulatedPerLanguage engines_TW[] = {
    {"zh", engines_zh_TW, std::size(engines_zh_TW)},
};

// Default locale to use if we only have the language.
// The first item of this list will be used if none match.
const PrepopulatedPerLanguage engines_unknown[] = {
    {"en", engines_en_US, std::size(engines_en_US)},
    {"be", engines_be_BY, std::size(engines_be_BY)},
    {"bg", engines_bg_BG, std::size(engines_bg_BG)},
    {"cs", engines_cs_CZ, std::size(engines_cs_CZ)},
    {"da", engines_da_DK, std::size(engines_da_DK)},
    {"de", engines_de_DE, std::size(engines_de_DE)},
    {"el", engines_el_GR, std::size(engines_el_GR)},
    {"es", engines_es_ES, std::size(engines_es_ES)},
    {"fi", engines_fi_FI, std::size(engines_fi_FI)},
    {"fr", engines_fr_FR, std::size(engines_fr_FR)},
    {"hr", engines_hr_HR, std::size(engines_hr_HR)},
    {"hu", engines_hu_HU, std::size(engines_hu_HU)},
    {"id", engines_id_ID, std::size(engines_id_ID)},
    {"it", engines_it_IT, std::size(engines_it_IT)},
    {"ja", engines_ja_JP, std::size(engines_ja_JP)},
    {"ko", engines_ko_KR, std::size(engines_ko_KR)},
    {"lt", engines_lt_LT, std::size(engines_lt_LT)},
    {"nb", engines_nb_NO, std::size(engines_nb_NO)},
    {"nn", engines_nn_NO, std::size(engines_nn_NO)},
    {"pl", engines_pl_PL, std::size(engines_pl_PL)},
    {"pt", engines_pt_BR, std::size(engines_pt_BR)},
    {"ro", engines_ro_RO, std::size(engines_ro_RO)},
    {"ru", engines_ru_RU, std::size(engines_ru_RU)},
    {"sv", engines_sv_SE, std::size(engines_sv_SE)},
    {"sk", engines_sk_SK, std::size(engines_sk_SK)},
    {"sr", engines_sr_RS, std::size(engines_sr_RS)},
    {"th", engines_th_TH, std::size(engines_th_TH)},
    {"tr", engines_tr_TR, std::size(engines_tr_TR)},
    {"uk", engines_uk_UA, std::size(engines_uk_UA)},
    {"vi", engines_vi_VN, std::size(engines_vi_VN)},
    {"zh", engines_zh_CN, std::size(engines_zh_CN)},
    {"zn", engines_zn_HK, std::size(engines_zn_HK)},
};
