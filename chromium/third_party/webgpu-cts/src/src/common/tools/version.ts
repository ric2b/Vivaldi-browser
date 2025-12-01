export const version = (() => {
  try {
    /* eslint-disable-next-line n/no-restricted-require */
    return require('child_process')
      .execSync('git describe --always --abbrev=0 --dirty')
      .toString()
      .trim();
  } catch {
    // Fail gracefully if git is not available.
    return 'unknown';
  }
})();
