import path from 'path';
import inject from '@rollup/plugin-inject';
import {nodeResolve} from '@rollup/plugin-node-resolve';
import terser from '@rollup/plugin-terser';
import cleanup from 'rollup-plugin-cleanup';

// plugin-node-resolve attempts to beautify code before rollup is done
// with tree shaking. To speed up the build time, we're excluding tfjs-
// partial which is tree-shaken from `@eyeo/mlaf`.
const beautify = () => cleanup({
  exclude: ['**/tfjs-partial.min.js', '**/hide-if-matches-xpath3-dependency*.js'],
  comments: 'none',
  maxEmptyLines: 1
});

// Ensure variable names aren't mangled for imported snippets
const preserveGlobals = () => inject({
  $: path.resolve('source/$.js'),
  hideElement: [path.resolve('source/utils/dom.js'), 'hideElement'],
  raceWinner: [path.resolve('source/introspection/race.js'), 'raceWinner']
});

export default [
  {
    input: './bundle/ml.js',
    plugins: [
      nodeResolve(),
      beautify(),
      preserveGlobals()
    ],
    output: {
      esModule: false,
      file: './dist/ml.source.js',
      format: 'commonjs'
    }
  },
  {
    input: './bundle/ml.js',
    plugins: [
      nodeResolve(),
      beautify(),
      terser(),
      preserveGlobals()
    ],
    output: {
      esModule: false,
      file: './dist/ml.js',
      format: 'commonjs'
    }
  }
];
