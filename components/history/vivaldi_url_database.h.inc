 public:
  // Vivaldi



  // History search ------------------------------------------------------------
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  history::DetailedUrlResults GetVivaldiDetailedHistory(const std::string query,
                              int max_results);

  history::TypedUrlResults GetVivaldiTypedHistory(
                        const std::string query,
                        int max_results);

bool CreateVivaldiURLsLastVisitIndex();

bool CreateVivaldiURLScoreIndex();
#endif
