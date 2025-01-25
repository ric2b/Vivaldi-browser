// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {RelatedWebsiteSetListItemElement} from './related_website_set_list_item.js';

export function getHtml(this: RelatedWebsiteSetListItemElement) {
  return html`
  <cr-expand-button id="expandButton" class="cr-row"
      ?expanded="${this.expanded}"
      @expanded-changed="${this.onExpandedChanged_}">
    <div id="container">
      <site-favicon id="favicon" domain="chrome://favicon2" aria-hidden="true">
      </site-favicon>
      <div id="borderPart">${this.boldQuery_(this.primarySite)}</div>
      <cr-icon class="icon" icon="cr20:domain"
          ?hidden="${this.isEnterpriseIconHidden_()}">
      </cr-icon>
    </div>
  </cr-expand-button>
  <cr-collapse id="expandedContent" ?opened="${this.expanded}">
    ${this.memberSites.map(item => html`
      <div class="cr-padded-text hr">
        <site-favicon id="favicon" domain="chrome://favicon2"
            aria-hidden="true">
        </site-favicon>
        <div class="cr-secondary-text">
          ${this.boldQuery_(item.site)} - ${this.getSiteType_(item.type)}
        </div>
      </div>`)}
  </cr-collapse>`;
}
