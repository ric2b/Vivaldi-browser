// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../../ui/legacy/legacy.js'; // for x-link

import * as Host from '../../../core/host/host.js';
import * as i18n from '../../../core/i18n/i18n.js';
import * as CodeMirror from '../../../third_party/codemirror.next/codemirror.next.js';
import * as TextEditor from '../../../ui/components/text_editor/text_editor.js';
import * as IconButton from '../../components/icon_button/icon_button.js';
import * as LitHtml from '../../lit-html/lit-html.js';
import * as VisualLogging from '../../visual_logging/visual_logging.js';

import styles from './codeBlock.css.js';

const UIStrings = {
  /**
   * @description The title of the button to copy the codeblock from a Markdown view.
   */
  copy: 'Copy code',
  /**
   * @description The title of the button after it was pressed and the text was copied to clipboard.
   */
  copied: 'Copied to clipboard',
  /**
   * @description Disclaimer shown in the code blocks.
   */
  disclaimer: 'Use code snippets with caution',
};
const str_ = i18n.i18n.registerUIStrings('ui/components/markdown_view/CodeBlock.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

export interface Heading {
  showCopyButton: boolean;
  text: string;
}

export class CodeBlock extends HTMLElement {
  static readonly litTagName = LitHtml.literal`devtools-code-block`;

  readonly #shadow = this.attachShadow({mode: 'open'});

  #code = '';
  #codeLang = '';
  #copyTimeout = 1000;
  #timer?: ReturnType<typeof setTimeout>;
  #copied = false;
  #editorState?: CodeMirror.EditorState;
  #languageConf = new CodeMirror.Compartment();
  /**
   * Whether to display a notice "​​Use code snippets with caution" in code
   * blocks.
   */
  #displayNotice = false;
  /**
   * Whether to display the toolbar on the top.
   */
  #displayToolbar = true;

  /**
   * Details of the `heading` of the codeblock right after toolbar.
   */
  #heading?: Heading;

  connectedCallback(): void {
    this.#shadow.adoptedStyleSheets = [styles];
    this.#render();
  }

  set code(value: string) {
    this.#code = value;
    this.#editorState = CodeMirror.EditorState.create({
      doc: this.#code,
      extensions: [
        TextEditor.Config.baseConfiguration(this.#code),
        CodeMirror.EditorState.readOnly.of(true),
        CodeMirror.EditorView.lineWrapping,
        this.#languageConf.of(CodeMirror.javascript.javascript()),
      ],
    });
    this.#render();
  }

  get code(): string {
    return this.#code;
  }

  set codeLang(value: string) {
    this.#codeLang = value;
    this.#render();
  }

  set timeout(value: number) {
    this.#copyTimeout = value;
    this.#render();
  }

  set displayNotice(value: boolean) {
    this.#displayNotice = value;
    this.#render();
  }

  set displayToolbar(value: boolean) {
    this.#displayToolbar = value;
    this.#render();
  }

  set heading(heading: Heading) {
    this.#heading = heading;
    this.#render();
  }

  #onCopy(): void {
    Host.InspectorFrontendHost.InspectorFrontendHostInstance.copyText(this.#code);
    this.#copied = true;
    this.#render();
    clearTimeout(this.#timer);
    this.#timer = setTimeout(() => {
      this.#copied = false;
      this.#render();
    }, this.#copyTimeout);
  }

  #renderCopyButton({noText = false}: {noText?: boolean} = {}): LitHtml.TemplateResult {
    const copyButtonClasses = LitHtml.Directives.classMap({
      copied: this.#copied,
      'copy-button': true,
    });

    // clang-format off
    return LitHtml.html`
      <button class=${copyButtonClasses}
        title=${i18nString(UIStrings.copy)}
        jslog=${VisualLogging.action('copy').track({click: true})}
        @click=${this.#onCopy}>
        <${IconButton.Icon.Icon.litTagName} name="copy"></${IconButton.Icon.Icon.litTagName}>
        ${(!noText || this.#copied) ? LitHtml.html`<span>${this.#copied ?
          i18nString(UIStrings.copied) :
          i18nString(UIStrings.copy)}</span>` : LitHtml.nothing}
      </button>
    `;
    // clang-format on
  }

  #renderToolbar(): LitHtml.TemplateResult {
    // clang-format off
    return LitHtml.html`<div class="toolbar" jslog=${VisualLogging.toolbar()}>
      <div class="lang">${this.#codeLang}</div>
      <div class="copy">
        ${this.#renderCopyButton()}
      </div>
    </div>`;
    // clang-format on
  }

  #renderNotice(): LitHtml.TemplateResult {
    // clang-format off
    return LitHtml.html`<p class="notice">
      <x-link class="link" href="https://support.google.com/legal/answer/13505487" jslog=${
        VisualLogging.link('code-disclaimer').track({
          click: true,
        })}>
        ${i18nString(UIStrings.disclaimer)}
      </x-link>
    </p>`;
    // clang-format on
  }

  #renderHeading(): LitHtml.LitTemplate {
    if (!this.#heading) {
      return LitHtml.nothing;
    }

    // clang-format off
    return LitHtml.html`
      <div class="heading">
        <h4 class="heading-text">${this.#heading.text}</h4>
        ${this.#heading.showCopyButton ? LitHtml.html`<div class="copy-button">
          ${this.#renderCopyButton({ noText: true })}
        </div>` : LitHtml.nothing}
      </div>
    `;
    // clang-format on
  }

  #render(): void {
    // clang-format off
    LitHtml.render(LitHtml.html`<div class='codeblock' jslog=${VisualLogging.section('code')}>
      ${this.#displayToolbar ? this.#renderToolbar() : LitHtml.nothing}
      <div class="editor-wrapper">
        ${this.#heading ? this.#renderHeading() : LitHtml.nothing}
        <div class="code">
          <${TextEditor.TextEditor.TextEditor.litTagName} .state=${
            this.#editorState
          }></${TextEditor.TextEditor.TextEditor.litTagName}>
        </div>
      </div>
      ${this.#displayNotice ? this.#renderNotice() : LitHtml.nothing}
    </div>`, this.#shadow, {
      host: this,
    });
    // clang-format on

    const editor = this.#shadow?.querySelector('devtools-text-editor')?.editor;

    if (!editor) {
      return;
    }
    let language = CodeMirror.html.html({autoCloseTags: false, selfClosingTags: true});
    switch (this.#codeLang) {
      case 'js':
        language = CodeMirror.javascript.javascript();
        break;
      case 'ts':
        language = CodeMirror.javascript.javascript({typescript: true});
        break;
      case 'jsx':
        language = CodeMirror.javascript.javascript({jsx: true});
        break;
      case 'css':
        language = CodeMirror.css.css();
        break;
    }
    editor.dispatch({
      effects: this.#languageConf.reconfigure(language),
    });
  }
}

customElements.define('devtools-code-block', CodeBlock);

declare global {
  interface HTMLElementTagNameMap {
    'devtools-code-block': CodeBlock;
  }
}
