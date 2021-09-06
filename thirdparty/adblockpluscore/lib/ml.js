/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-present eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

/** @module */

"use strict";

/**
 * @typedef {number|Array.<module:ml~InputData>} InputData
 */

/**
 * @typedef {object} InputObject
 * @property {Array.<module:ml~InputData>} data The data to feed into the model.
 * @property {Array.<module:ml~PreprocessingObject>} preprocess A list of
 * preprocessing steps in `{@link module:ml~PreprocessingObject}` format.
 */

/**
 * @typedef {object} PreprocessingObject
 * @property {string} funcName The name of a preprocessing function.
 * @property {?*} [args] Arguments for the preprocessing function.
 */

/**
 * An `ML` object implements machine learning based on
 * {@link https://www.tensorflow.org/js TensorFlow.js}.
 *
 * Currently these preprocessing functions are supported:
 *
 * - `cast` - cast tensor into a type provided as an argument
 * - `stack` - {@link https://js.tensorflow.org/api/latest/#stack tf.stack}
 * - `unstack` - {@link https://js.tensorflow.org/api/latest/#unstack tf.unstack}
 * - `localPooling` - run local pooling on each element in an array
 * - `pad` - {@link https://js.tensorflow.org/api/latest/#pad tf.pad}
 * - `oneHot` - {@link https://js.tensorflow.org/api/latest/#oneHot tf.oneHot}
 *
 * @example <caption>Simple usage</caption>
 * let tf = require("@tensorflow/tfjs");
 * let ml = new ML(tf);
 * // Assuming a pretrained model that predicts (y = 2x - 1)
 *
 * // See here: https://github.com/tensorflow/tfjs-examples/blob/master/getting-started/index.js
 * ml.modelURL = "https://example.com/model.json";
 * console.log(await ml.predict([{data: [[20]]}]));
 * // Outputs approximately 39
 *
 * @example <caption>Usage with preprocessing options</caption>
 * let tf = require("@tensorflow/tfjs");
 * let ml = new ML(tf);
 * // Assuming a graph classification model
 * ml.modelURL = "https://example.com/model.json";
 * let adjacencyMatrices = [[0, 1, 0],
 *                          [1, 0, 1],
 *                          [0, 1, 0]];
 * let numberOfFeatures = 100;
 * let graphSize = 10;
 * let preprocessAdjacency = [
 *   {funcName: "pad", args: graphSize}
 *   {funcName: "unstack"}
 *   {funcName: "localPooling"}
 *   {funcName: "stack"}
 * ];
 * let featureVectors = [[1], [2], [3]];
 * let preprocessFeatures = [
 *   {funcName: "cast", args: "int32"}
 *   {funcName: "pad", args: graphSize}
 *   {funcName: "oneHot", args: numberOfFeatures}
 *   {funcName: "cast", args: "float32"}
 * ];
 * console.log(await ml.predict([
 *   {data: adjacencyMatrices, preprocess: preprocessAdjacency},
 *   {data: featureVectors, preprocess: preprocessFeatures}
 * ]));
 */
exports.ML = class ML
{
  /**
   * @param {object} lib TensorFlow.js library to use for ML operations.
   */
  constructor(lib)
  {
    this._modelURL = null;
    this._model = null;

    if (lib == null)
      throw new Error("lib must not be null.");

    this._tf = lib;

    this._preprocessingFunctions = new Map([
      ["cast", this._tf.cast],
      ["stack", this._tf.stack],
      ["unstack", this._tf.unstack],
      ["localPooling", this._localPoolingMap.bind(this)],
      ["pad", this._padTensor.bind(this)],
      ["oneHot", this._oneHotEncode.bind(this)]
    ]);
  }

  /**
   * A URL to the model manifest in `tfjs_graphmodel` format.
   * @type {string}
   */
  get modelURL()
  {
    return this._modelURL;
  }

  set modelURL(value)
  {
    if (this._modelURL != value)
    {
      if (this._model)
      {
        this._model.dispose();
        this._model = null;
      }
      this._modelURL = value;
    }
  }

  /**
   * Runs prediction based on provided inputs.
   * @param {Array.<module:ml~InputObject>} inputs
   *   `{@link module:ml~InputObject}` instances for prediction.
   * @returns {Promise.<Array.<number>>} A promise that fulfills with raw
   *   results from the model.
   */
  async predict(inputs)
  {
    if (!this._model)
    {
      await this._initModel();
      return this.predict(inputs);
    }

    let preprocessed = this._preprocess(inputs);

    try
    {
      let result = await this._model.executeAsync(preprocessed);

      try
      {
        return result.dataSync();
      }
      finally
      {
        result.dispose();
      }
    }
    finally
    {
      for (let inputTensor of preprocessed)
        inputTensor.dispose();
    }
  }

  /**
   * Initializes the model from a provided URL.
   * @returns {Promise}
   * @private
   */
  async _initModel()
  {
    if (!this._modelURL)
      throw new Error("modelURL must be set to a URL.");

    this._tf.enableProdMode();
    this._model = await this._tf.loadGraphModel(this._modelURL);
  }

  /**
   * Prepares the inputs for running inference according to the preprocessing
   * rules defined for each.
   * @param {Array.<module:ml~InputObject>} inputs
   *   {@link module:ml~InputObject} instances for preprocessing.
   * @returns {Array.<tf.Tensor>}
   * @private
   */
  _preprocess(inputs)
  {
    return this._tf.tidy(() =>
    {
      return inputs.map(input =>
      {
        let preprocessed = this._tf.tensor(input.data);
        if (input.preprocess)
        {
          for (let params of input.preprocess)
          {
            preprocessed = this._preprocessingFunctions.get(params.funcName)(
              preprocessed, params.args
            );
          }
        }
        return preprocessed;
      });
    });
  }

  /**
   * Computes a pooling operation for a matrix `A: (D^(-0.5))*(A)*(D^(-0.5))`
   * where `D` is the diagonal node degree matrix.
   *
   * Based on Kipf & Welling (2017).
   *
   * See {@link https://github.com/danielegrattarola/spektral/blob/a3883117b16b958e2e24723afc6885fbc7df397a/spektral/utils/convolution.py#L88 Python implementation}
   * @param {tf.Tensor} adjacencyMatrix
   * @returns {tf.Tensor} The normalized adjacency matrix.
   * @private
   */
  _localPooling(adjacencyMatrix)
  {
    let tf = this._tf;
    let aTilde = adjacencyMatrix.add(tf.eye(adjacencyMatrix.shape[0]));
    let degreePow = tf.pow(tf.sum(aTilde, 1), -0.5);
    degreePow = degreePow.where(
      tf.isFinite(degreePow),
      tf.zeros(degreePow.shape)
    );
    let degreeMx = degreePow.mul(tf.eye(aTilde.shape[0]));
    let normalized = degreeMx.dot(aTilde).dot(degreeMx);
    return normalized;
  }

  /**
   * Runs `{@link module:ml.ML#_localPooling}` on an array of tensors.
   * @param {Array.<tf.Tensor>} adjacencyMatrices
   * @returns {Array.<tf.Tensor>}
   * @private
   */
  _localPoolingMap(adjacencyMatrices)
  {
    return adjacencyMatrices.map(x => this._localPooling(x));
  }

  /**
   * Pads tensor on the right with `0`s.
   *
   * See {@link https://js.tensorflow.org/api/latest/#pad here} for details.
   * @param {tf.Tensor} tensor
   * @param {number} padSize
   * @returns {tf.Tensor}
   * @private
   *
   * @example
   * // returns [[1, 1, 1, 0, 0],
   * //          [1, 1, 1, 0, 0]]
   * _padTensor([[1, 1, 1],
   *             [1, 1, 1]],
   *             5
   *           );
   */
  _padTensor(tensor, padSize)
  {
    let paddings = tensor.shape.map(x => [0, padSize - x]);
    paddings[0] = [0, 0];
    return tensor.pad(paddings);
  }

  /**
   * One-hot encodes tensor (place 1 on a relevant position in a vector).
   *
   * See {@link https://js.tensorflow.org/api/latest/#oneHot here} for details.
   * @param {tf.Tensor} tensor
   * @param {number} numOfClasses Number of output classes.
   * @returns {tf.Tensor}
   * @private
   *
   * @example
   * //  returns [
   * //    [1, 0, 0, 0],
   * //    [0, 1, 0, 0],
   * //    [0, 0, 1, 0]
   * //  ]
   * _oneHotEncode([0, 1, 2], 4);
   */
  _oneHotEncode(tensor, numOfClasses)
  {
    return this._tf.oneHot(tensor, numOfClasses);
  }
};
