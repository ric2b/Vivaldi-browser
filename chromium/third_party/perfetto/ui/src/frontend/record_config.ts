// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {exists} from '../base/utils';
import {getDefaultRecordingTargets, RecordingTarget} from '../common/state';
import {
  createEmptyRecordConfig,
  NamedRecordConfig,
  NAMED_RECORD_CONFIG_SCHEMA,
  RecordConfig,
  RECORD_CONFIG_SCHEMA,
} from '../controller/record_config_types';

const LOCAL_STORAGE_RECORD_CONFIGS_KEY = 'recordConfigs';
const LOCAL_STORAGE_AUTOSAVE_CONFIG_KEY = 'autosaveConfig';
const LOCAL_STORAGE_RECORD_TARGET_OS_KEY = 'recordTargetOS';

export class RecordConfigStore {
  recordConfigs: Array<NamedRecordConfig>;
  recordConfigNames: Set<string>;

  constructor() {
    this.recordConfigs = [];
    this.recordConfigNames = new Set();
    this.reloadFromLocalStorage();
  }

  private _save() {
    window.localStorage.setItem(
      LOCAL_STORAGE_RECORD_CONFIGS_KEY,
      JSON.stringify(this.recordConfigs),
    );
  }

  save(recordConfig: RecordConfig, title?: string): void {
    // We reload from local storage in case of concurrent
    // modifications of local storage from a different tab.
    this.reloadFromLocalStorage();

    const savedTitle = title ?? new Date().toJSON();
    const config: NamedRecordConfig = {
      title: savedTitle,
      config: recordConfig,
      key: new Date().toJSON(),
    };

    this.recordConfigs.push(config);
    this.recordConfigNames.add(savedTitle);

    this._save();
  }

  overwrite(recordConfig: RecordConfig, key: string) {
    // We reload from local storage in case of concurrent
    // modifications of local storage from a different tab.
    this.reloadFromLocalStorage();

    const found = this.recordConfigs.find((e) => e.key === key);
    if (found === undefined) {
      throw new Error('trying to overwrite non-existing config');
    }

    found.config = recordConfig;

    this._save();
  }

  delete(key: string): void {
    // We reload from local storage in case of concurrent
    // modifications of local storage from a different tab.
    this.reloadFromLocalStorage();

    let idx = -1;
    for (let i = 0; i < this.recordConfigs.length; ++i) {
      if (this.recordConfigs[i].key === key) {
        idx = i;
        break;
      }
    }

    if (idx !== -1) {
      this.recordConfigNames.delete(this.recordConfigs[idx].title);
      this.recordConfigs.splice(idx, 1);
      this._save();
    } else {
      // TODO(bsebastien): Show a warning message to the user in the UI.
      console.warn("The config selected doesn't exist any more");
    }
  }

  private clearRecordConfigs(): void {
    this.recordConfigs = [];
    this.recordConfigNames.clear();
    this._save();
  }

  reloadFromLocalStorage(): void {
    const configsLocalStorage = window.localStorage.getItem(
      LOCAL_STORAGE_RECORD_CONFIGS_KEY,
    );

    if (exists(configsLocalStorage)) {
      this.recordConfigNames.clear();

      try {
        const validConfigLocalStorage: Array<NamedRecordConfig> = [];
        const parsedConfigsLocalStorage = JSON.parse(configsLocalStorage);

        // Check if it's an array.
        if (!Array.isArray(parsedConfigsLocalStorage)) {
          this.clearRecordConfigs();
          return;
        }

        for (let i = 0; i < parsedConfigsLocalStorage.length; ++i) {
          const serConfig = parsedConfigsLocalStorage[i];
          const res = NAMED_RECORD_CONFIG_SCHEMA.safeParse(serConfig);
          if (res.success) {
            validConfigLocalStorage.push(res.data);
          } else {
            console.log(
              'Validation of saved record config has failed: ',
              res.error.toString(),
            );
          }
        }

        this.recordConfigs = validConfigLocalStorage;
        this._save();
      } catch (e) {
        this.clearRecordConfigs();
      }
    } else {
      this.clearRecordConfigs();
    }
  }

  canSave(title: string) {
    return !this.recordConfigNames.has(title);
  }
}

// This class is a singleton to avoid many instances
// conflicting as they attempt to edit localStorage.
export const recordConfigStore = new RecordConfigStore();

export class AutosaveConfigStore {
  private config: RecordConfig;

  // Whether the current config is a default one or has been saved before.
  // Used to determine whether the button to load "last started config" should
  // be present in the recording profiles list.
  hasSavedConfig: boolean;

  constructor() {
    this.hasSavedConfig = false;
    this.config = createEmptyRecordConfig();
    const savedItem = window.localStorage.getItem(
      LOCAL_STORAGE_AUTOSAVE_CONFIG_KEY,
    );
    if (savedItem === null) {
      return;
    }
    const parsed = JSON.parse(savedItem);
    if (parsed !== null && typeof parsed === 'object') {
      const res = RECORD_CONFIG_SCHEMA.safeParse(parsed);
      if (res.success) {
        this.config = res.data;
        this.hasSavedConfig = true;
      }
    }
  }

  get(): RecordConfig {
    return this.config;
  }

  save(newConfig: RecordConfig) {
    window.localStorage.setItem(
      LOCAL_STORAGE_AUTOSAVE_CONFIG_KEY,
      JSON.stringify(newConfig),
    );
    this.config = newConfig;
    this.hasSavedConfig = true;
  }
}

export const autosaveConfigStore = new AutosaveConfigStore();

export class RecordTargetStore {
  recordTargetOS: string | null;

  constructor() {
    this.recordTargetOS = window.localStorage.getItem(
      LOCAL_STORAGE_RECORD_TARGET_OS_KEY,
    );
  }

  get(): string | null {
    return this.recordTargetOS;
  }

  getValidTarget(): RecordingTarget {
    const validTargets = getDefaultRecordingTargets();
    const savedOS = this.get();

    const validSavedTarget = validTargets.find((el) => el.os === savedOS);
    return validSavedTarget || validTargets[0];
  }

  save(newTargetOS: string) {
    window.localStorage.setItem(
      LOCAL_STORAGE_RECORD_TARGET_OS_KEY,
      newTargetOS,
    );
    this.recordTargetOS = newTargetOS;
  }
}

export const recordTargetStore = new RecordTargetStore();
