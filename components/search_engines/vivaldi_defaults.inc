// Copyright 2021 Vivaldi Technologies AS. All rights reserved.

// Search engines by locale. Every entry here MUST feature the default and
// private default search engines in first and second position and they
// must be repeated in the remainder of the list. Search engines will be ordered
// based on the order of these lists, excluding the first two entries.
const PrepopulatedEngine* const engines_default[] = {
    &duckduckgo,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia,
    &google,
};

const PrepopulatedEngine* const engines_be_BY[] = {
    &yandex_by,  // default
    &startpage_com,  // defaultPrivate
    &yandex_by,
    &google,
    &ozon,
    &wikipedia_be,
    &ebay,
    &duckduckgo,
    &ecosia_by,
    &startpage_com,
    &wolfram,
};
const PrepopulatedEngine* const engines_bg_BG[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_bg,
    &google,
};
const PrepopulatedEngine* const engines_cs_CZ[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_cs,
    &google,
};
const PrepopulatedEngine* const engines_da_DK[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_dk,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_da,
    &google,
};
const PrepopulatedEngine* const engines_de_DE[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_de,
    &duckduckgo,
    &ecosia_de,
    &qwant_de,
    &you,
    &startpage_com,
    &wikipedia_de,
    &google,
};
const PrepopulatedEngine* const engines_de_AT[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_at,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &google,
    &wikipedia_de,
};
const PrepopulatedEngine* const engines_de_CH[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_ch,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_de,
    &google,
};
const PrepopulatedEngine* const engines_el_GR[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_el,
    &google,
};
const PrepopulatedEngine* const engines_en_CA[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_ca,
    &duckduckgo,
    &ecosia_ca,
    &qwant_ca,
    &you,
    &startpage_com,
    &google,
    &wikipedia,
};
const PrepopulatedEngine* const engines_en_GB[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_uk,
    &duckduckgo,
    &ecosia_gb,
    &qwant_gb,
    &you,
    &startpage_com,
    &wikipedia,
    &google,
};
const PrepopulatedEngine* const engines_en_IE[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_ie,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &google,
    &wikipedia,
};
const PrepopulatedEngine* const engines_en_IN[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_in,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &google,
    &wikipedia,
};
const PrepopulatedEngine* const engines_en_PH[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &yahoo_ph,
    &bing,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &google,
    &wikipedia,
};
const PrepopulatedEngine* const engines_en_SG[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &yahoo_sg,
    &bing,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &google,
    &wikipedia,
};
const PrepopulatedEngine* const engines_en_US[] = {
    &bing,  // default
    &duckduckgo,  // defaultPrivate
    &bing,
    &yahoo_us,
    &duckduckgo,
    &yelp,
    &ecosia_us,
    &qwant_us,
    &you,
    &startpage_com,
    &wikipedia,
    &google,
};
const PrepopulatedEngine* const engines_es_ES[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_es,
    &duckduckgo,
    &ecosia,
    &qwant_es,
    &startpage_com,
    &wikipedia_es,
    &google,
};
const PrepopulatedEngine* const engines_es_AR[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &yahoo_ar,
    &bing,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &google,
    &wikipedia_es,
};
const PrepopulatedEngine* const engines_es_CL[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &yahoo_cl,
    &bing,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &google,
    &wikipedia_es,
};
const PrepopulatedEngine* const engines_es_CO[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &yahoo_co,
    &bing,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &google,
    &wikipedia_es,
};
const PrepopulatedEngine* const engines_es_PE[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &yahoo_pe,
    &bing,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &google,
    &wikipedia_es,
};
const PrepopulatedEngine* const engines_es_MX[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &yahoo_mx,
    &bing,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &google,
    &wikipedia_es,
};
const PrepopulatedEngine* const engines_es_VE[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &yahoo_ve,
    &bing,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &google,
    &wikipedia_es,
};
const PrepopulatedEngine* const engines_fi_FI[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_fi,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_fi,
    &google,
};
const PrepopulatedEngine* const engines_fr_FR[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_fr,
    &duckduckgo,
    &ecosia_fr,
    &qwant_fr,
    &startpage_com,
    &wikipedia_fr,
    &google,
};
const PrepopulatedEngine* const engines_hr_HR[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_hr,
    &google,
};
const PrepopulatedEngine* const engines_hu_HU[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_hu,
    &google,
};
const PrepopulatedEngine* const engines_id_ID[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_id,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_id,
    &google,
};
const PrepopulatedEngine* const engines_it_IT[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_it,
    &duckduckgo,
    &ecosia,
    &qwant_it,
    &startpage_com,
    &wikipedia_it,
    &google,
};
const PrepopulatedEngine* const engines_ja_JP[] = {
    &bing,  // default
    &duckduckgo,  // defaultPrivate
    &bing,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_jp,
    &google,
};
const PrepopulatedEngine* const engines_ko_KR[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_ko,
    &google,
};
const PrepopulatedEngine* const engines_lt_LT[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_lt,
    &google,
};
const PrepopulatedEngine* const engines_nb_NO[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_no,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_no,
    &google,
};
const PrepopulatedEngine* const engines_nn_NO[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_no,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_nn,
    &google,
};
const PrepopulatedEngine* const engines_pl_PL[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_pl,
    &google,
};
const PrepopulatedEngine* const engines_pt_BR[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_br,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_pt,
    &google,
};
const PrepopulatedEngine* const engines_pt_PT[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_pt,
    &google,
};
const PrepopulatedEngine* const engines_ro_RO[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_ro,
    &google,
};
const PrepopulatedEngine* const engines_ru_RU[] = {
    &yandex_ru,  // default
    &duckduckgo,  // defaultPrivate
    &yandex_ru,
    &google,
    &wikipedia_ru,
    &duckduckgo,
    &mail_ru,
    &ecosia,
    &startpage_com,
    &wolfram,
};
const PrepopulatedEngine* const engines_sv_SE[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_se,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_se,
    &google,
};
const PrepopulatedEngine* const engines_sk_SK[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_sk,
    &google,
};
const PrepopulatedEngine* const engines_sr_RS[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_sr,
    &google,
};
const PrepopulatedEngine* const engines_th_TH[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &yahoo_th,
    &bing,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &google,
    &wikipedia_th,
};
const PrepopulatedEngine* const engines_tr_TR[] = {
    &duckduckgo,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yandex_tr,
    &google,
    &amazon,
    &wikipedia_tr,
    &ebay,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wolfram,
};
const PrepopulatedEngine* const engines_uk_UA[] = {
    &duckduckgo,  // default
    &startpage_com,  // defaultPrivate
    &google,
    &wikipedia_uk,
    &ebay,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wolfram,
};
const PrepopulatedEngine* const engines_vi_VN[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_vn,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_vi,
    &google,
};
const PrepopulatedEngine* const engines_zh_CN[] = {
    &bing,  // default
    &yahoo,  // defaultPrivate
    &bing,
    &yahoo,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_zh,
    &google,
};
const PrepopulatedEngine* const engines_zn_HK[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &yahoo_hk,
    &bing,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &google,
    &wikipedia_zh,
};
const PrepopulatedEngine* const engines_zh_TW[] = {
    &bing,  // default
    &startpage_com,  // defaultPrivate
    &bing,
    &yahoo_tw,
    &duckduckgo,
    &ecosia,
    &startpage_com,
    &wikipedia_zh,
    &google,
};

const PrepopulatedEngine* const engines_unittests[] = {
    &google_unittests,
    &google_unittests,
    &google_unittests,
    &bing_unittests,
    &yahoo_unittests,
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
