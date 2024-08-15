import { unreachable } from '../../util/util.js';

let windowURL: URL | undefined = undefined;
function getWindowURL() {
  if (windowURL === undefined) {
    windowURL = new URL(window.location.toString());
  }
  return windowURL;
}

/** Parse a runner option that is always boolean-typed. False if missing or '0'. */
export function optionEnabled(
  opt: string,
  searchParams: URLSearchParams = getWindowURL().searchParams
): boolean {
  const val = searchParams.get(opt);
  return val !== null && val !== '0';
}

/** Parse a runner option that is always string-typed. If the option is missing, returns `''`. */
export function optionString(
  opt: string,
  searchParams: URLSearchParams = getWindowURL().searchParams
): string {
  return searchParams.get(opt) || '';
}

/** Runtime modes for whether to run tests in a worker. '0' means no worker. */
type WorkerMode = '0' | 'dedicated' | 'service' | 'shared';
/** Parse a runner option for different worker modes (as in `?worker=shared`). */
export function optionWorkerMode(
  opt: string,
  searchParams: URLSearchParams = getWindowURL().searchParams
): WorkerMode {
  const value = searchParams.get(opt);
  if (value === null || value === '0') {
    return '0';
  } else if (value === 'service') {
    return 'service';
  } else if (value === 'shared') {
    return 'shared';
  } else if (value === '' || value === '1' || value === 'dedicated') {
    return 'dedicated';
  }
  unreachable('invalid worker= option value');
}

/**
 * The possible options for the tests.
 */
export interface CTSOptions {
  worker: WorkerMode;
  debug: boolean;
  compatibility: boolean;
  forceFallbackAdapter: boolean;
  unrollConstEvalLoops: boolean;
  powerPreference: GPUPowerPreference | '';
}

export const kDefaultCTSOptions: CTSOptions = {
  worker: '0',
  debug: true,
  compatibility: false,
  forceFallbackAdapter: false,
  unrollConstEvalLoops: false,
  powerPreference: '',
};

/**
 * Extra per option info.
 */
export interface OptionInfo {
  description: string;
  parser?: (key: string, searchParams?: URLSearchParams) => boolean | string;
  selectValueDescriptions?: { value: string; description: string }[];
}

/**
 * Type for info for every option. This definition means adding an option
 * will generate a compile time error if no extra info is provided.
 */
export type OptionsInfos<Type> = Record<keyof Type, OptionInfo>;

/**
 * Options to the CTS.
 */
export const kCTSOptionsInfo: OptionsInfos<CTSOptions> = {
  worker: {
    description: 'run in a worker',
    parser: optionWorkerMode,
    selectValueDescriptions: [
      { value: '0', description: 'no worker' },
      { value: 'dedicated', description: 'dedicated worker' },
      { value: 'shared', description: 'shared worker' },
      { value: 'service', description: 'service worker' },
    ],
  },
  debug: { description: 'show more info' },
  compatibility: { description: 'run in compatibility mode' },
  forceFallbackAdapter: { description: 'pass forceFallbackAdapter: true to requestAdapter' },
  unrollConstEvalLoops: { description: 'unroll const eval loops in WGSL' },
  powerPreference: {
    description: 'set default powerPreference for some tests',
    parser: optionString,
    selectValueDescriptions: [
      { value: '', description: 'default' },
      { value: 'low-power', description: 'low-power' },
      { value: 'high-performance', description: 'high-performance' },
    ],
  },
};

/**
 * Converts camel case to snake case.
 * Examples:
 *    fooBar -> foo_bar
 *    parseHTMLFile -> parse_html_file
 */
export function camelCaseToSnakeCase(id: string) {
  return id
    .replace(/(.)([A-Z][a-z]+)/g, '$1_$2')
    .replace(/([a-z0-9])([A-Z])/g, '$1_$2')
    .toLowerCase();
}

/**
 * Creates a Options from search parameters.
 */
function getOptionsInfoFromSearchString<Type extends CTSOptions>(
  optionsInfos: OptionsInfos<Type>,
  searchString: string
): Type {
  const searchParams = new URLSearchParams(searchString);
  const optionValues: Record<string, boolean | string> = {};
  for (const [optionName, info] of Object.entries(optionsInfos)) {
    const parser = info.parser || optionEnabled;
    optionValues[optionName] = parser(camelCaseToSnakeCase(optionName), searchParams);
  }
  return optionValues as unknown as Type;
}

/**
 * Given a test query string in the form of `suite:foo,bar,moo&opt1=val1&opt2=val2
 * returns the query and the options.
 */
export function parseSearchParamLikeWithOptions<Type extends CTSOptions>(
  optionsInfos: OptionsInfos<Type>,
  query: string
): {
  queries: string[];
  options: Type;
} {
  const searchString = query.includes('q=') || query.startsWith('?') ? query : `q=${query}`;
  const queries = new URLSearchParams(searchString).getAll('q');
  const options = getOptionsInfoFromSearchString(optionsInfos, searchString);
  return { queries, options };
}

/**
 * Given a test query string in the form of `suite:foo,bar,moo&opt1=val1&opt2=val2
 * returns the query and the common options.
 */
export function parseSearchParamLikeWithCTSOptions(query: string) {
  return parseSearchParamLikeWithOptions(kCTSOptionsInfo, query);
}
