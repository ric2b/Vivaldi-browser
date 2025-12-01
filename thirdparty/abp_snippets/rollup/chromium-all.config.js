import {nodeResolve} from '@rollup/plugin-node-resolve';
import terser from '@rollup/plugin-terser';
import cleanup from 'rollup-plugin-cleanup';

const beautify = () => cleanup({
  exclude: ['**/hide-if-matches-xpath3-dependency*.js'],
  comments: 'none',
  maxEmptyLines: 1
});

export default [
  {
    input: './bundle/isolated-full.js',
    plugins: [
      nodeResolve(),
      beautify()
    ],
    output: {
      esModule: false,
      file: './dist/isolated-all.source.js',
      format: 'commonjs'
    }
  },
  {
    input: './bundle/isolated-full.js',
    plugins: [
      nodeResolve(),
      beautify(),
      terser()
    ],
    output: {
      esModule: false,
      file: './dist/isolated-all.js',
      format: 'commonjs'
    }
  }
];
