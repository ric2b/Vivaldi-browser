import {nodeResolve} from '@rollup/plugin-node-resolve';
import terser from '@rollup/plugin-terser';
import cleanup from 'rollup-plugin-cleanup';

// plugin-node-resolve attempts to beautify code before rollup is done
// with tree shaking.
const beautify = () => cleanup({
  exclude: ['**/hide-if-matches-xpath3-dependency*.js'],
  comments: 'none',
  maxEmptyLines: 1
});

export default [
  {
    input: './bundle/webext-isolated-full.js',
    plugins: [
      nodeResolve(),
      beautify()
    ],
    output: {
      esModule: false,
      file: './dist/isolated-full.source.js',
      format: 'commonjs'
    }
  },
  {
    input: './bundle/webext-isolated-full.js',
    plugins: [
      nodeResolve(),
      beautify(),
      terser()
    ],
    output: {
      esModule: false,
      file: './dist/isolated-full.js',
      format: 'commonjs'
    }
  }
];
