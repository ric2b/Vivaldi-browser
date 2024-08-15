import {nodeResolve} from '@rollup/plugin-node-resolve';
import terser from '@rollup/plugin-terser';
import cleanup from 'rollup-plugin-cleanup';

const beautify = () => cleanup({
  comments: 'none',
  maxEmptyLines: 1
});

export default [
  {
    input: './bundle/main.js',
    plugins: [
      nodeResolve(),
      beautify()
    ],
    output: {
      esModule: false,
      file: './dist/main.source.js',
      format: 'commonjs'
    }
  },
  {
    input: './bundle/main.js',
    plugins: [
      nodeResolve(),
      beautify(),
      terser()
    ],
    output: {
      esModule: false,
      file: './dist/main.js',
      format: 'commonjs'
    }
  }
];
