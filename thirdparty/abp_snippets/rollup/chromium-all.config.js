import path from 'path';
import inject from '@rollup/plugin-inject';
import {nodeResolve} from '@rollup/plugin-node-resolve';
import terser from '@rollup/plugin-terser';
import cleanup from 'rollup-plugin-cleanup';

const beautify = () => cleanup({
  exclude: ['**/tfjs-partial.min.js', '**/hide-if-matches-xpath3-dependency*.js'],
  comments: 'none',
  maxEmptyLines: 1
});

const preserveGlobals = () => inject({
  $: path.resolve('source/$.js'),
  hideElement: [path.resolve('source/utils/dom.js'), 'hideElement'],
  raceWinner: [path.resolve('source/introspection/race.js'), 'raceWinner']
});

export default [
  {
    input: './bundle/isolated-first-all.js',
    plugins: [
      nodeResolve(),
      beautify(),
      preserveGlobals()
    ],
    output: {
      esModule: false,
      file: './dist/isolated-all.source.js',
      format: 'commonjs'
    }
  },
  {
    input: './bundle/isolated-first-all.js',
    plugins: [
      nodeResolve(),
      beautify(),
      terser(),
      preserveGlobals()
    ],
    output: {
      esModule: false,
      file: './dist/isolated-all.js',
      format: 'commonjs'
    }
  }
];
