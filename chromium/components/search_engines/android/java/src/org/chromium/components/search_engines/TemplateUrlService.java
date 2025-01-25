// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Android wrapper of the TemplateUrlService which provides access from the Java
 * layer.
 *
 * Only usable from the UI thread as it's primary purpose is for supporting the Android
 * preferences UI.
 *
 * See components/search_engines/template_url_service.h for more details.
 */
@SuppressWarnings("EnumOrdinal") // Vivaldi
public class TemplateUrlService {
    public enum DefaultSearchType {
        DEFAULT_SEARCH_MAIN,
        DEFAULT_SEARCH_PRIVATE,
        DEFAULT_SEARCH_FIELD,
        DEFAULT_SEARCH_FIELD_PRIVATE,
        DEFAULT_SEARCH_SPEED_DIALS,
        DEFAULT_SEARCH_SPEED_DIALS_PRIVATE,
        DEFAULT_SEARCH_IMAGE
    }

    public static class PostParams {
        public String contentType;
        public byte[] postContent;
    }

    /** This listener will be notified when template url service is done loading. */
    public interface LoadListener {
        void onTemplateUrlServiceLoaded();
    }

    /** Observer to be notified whenever the set of TemplateURLs are modified. */
    public interface TemplateUrlServiceObserver {
        /** Notification that the template url model has changed in some way. */
        void onTemplateURLServiceChanged();
    }

    private final ObserverList<LoadListener> mLoadListeners = new ObserverList<>();
    private final ObserverList<TemplateUrlServiceObserver> mObservers = new ObserverList<>();
    private long mNativeTemplateUrlServiceAndroid;

    private TemplateUrlService(long nativeTemplateUrlServiceAndroid) {
        // Note that this technically leaks the native object, however, TemplateUrlService
        // is a singleton that lives forever and there's no clean shutdown of Chrome on Android.
        mNativeTemplateUrlServiceAndroid = nativeTemplateUrlServiceAndroid;
    }

    @CalledByNative
    private static TemplateUrlService create(long nativeTemplateUrlServiceAndroid) {
        return new TemplateUrlService(nativeTemplateUrlServiceAndroid);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeTemplateUrlServiceAndroid = 0;
    }

    public boolean isLoaded() {
        ThreadUtils.assertOnUiThread();
        return TemplateUrlServiceJni.get()
                .isLoaded(mNativeTemplateUrlServiceAndroid, TemplateUrlService.this);
    }

    public void load() {
        ThreadUtils.assertOnUiThread();
        TemplateUrlServiceJni.get().load(mNativeTemplateUrlServiceAndroid, TemplateUrlService.this);
    }

    /**
     * Ensure the TemplateUrlService is loaded before running the specified action.  If the service
     * is already loaded, then run the action immediately.
     * <p>
     * Because this can introduce an arbitrary delay in the action being executed, ensure the state
     * is still valid in the action before interacting with anything that might no longer be
     * available (i.e. an Activity that has since been destroyed).
     *
     * @param action The action to be run.
     */
    public void runWhenLoaded(final Runnable action) {
        if (isLoaded()) {
            action.run();
        } else {
            registerLoadListener(
                    new LoadListener() {
                        @Override
                        public void onTemplateUrlServiceLoaded() {
                            unregisterLoadListener(this);
                            action.run();
                        }
                    });
            load();
        }
    }

    /** Returns a list of the all available search engines. */
    public List<TemplateUrl> getTemplateUrls() {
        ThreadUtils.assertOnUiThread();
        List<TemplateUrl> templateUrls = new ArrayList<>();
        TemplateUrlServiceJni.get()
                .getTemplateUrls(
                        mNativeTemplateUrlServiceAndroid, TemplateUrlService.this, templateUrls);
        return templateUrls;
    }

    /**
     * Called from native to populate the list of all available search engines.
     * @param templateUrls The list of {@link TemplateUrl} to be added.
     * @param templateUrl The {@link TemplateUrl} would add to the list.
     */
    @CalledByNative
    private static void addTemplateUrlToList(
            List<TemplateUrl> templateUrls, TemplateUrl templateUrl) {
        templateUrls.add(templateUrl);
    }

    /** Called from native when template URL service is done loading. */
    @CalledByNative
    private void templateUrlServiceLoaded() {
        ThreadUtils.assertOnUiThread();
        for (LoadListener listener : mLoadListeners) {
            listener.onTemplateUrlServiceLoaded();
        }
    }

    @CalledByNative
    private void onTemplateURLServiceChanged() {
        for (TemplateUrlServiceObserver observer : mObservers) {
            observer.onTemplateURLServiceChanged();
        }
    }

    /**
     * @return {@link TemplateUrl} for the default search engine.  This can
     *         be null if DSEs are disabled entirely by administrators.
     */
    public @Nullable TemplateUrl getDefaultSearchEngineTemplateUrl() {
        return getDefaultSearchEngineTemplateUrl(DefaultSearchType.DEFAULT_SEARCH_MAIN);
    }

    public @Nullable TemplateUrl getDefaultSearchEngineTemplateUrl(DefaultSearchType type) {
        if (!isLoaded()) return null;
        return TemplateUrlServiceJni.get()
                .getDefaultSearchEngine(mNativeTemplateUrlServiceAndroid, TemplateUrlService.this);
    }

    public void setSearchEngine(String selectedKeyword, @ChoiceMadeLocation int choiceLocation) {
        ThreadUtils.assertOnUiThread();
        TemplateUrlServiceJni.get()
                .setUserSelectedDefaultSearchProvider(
                        mNativeTemplateUrlServiceAndroid,
                        TemplateUrlService.this,
                        selectedKeyword,
                        choiceLocation,
                        DefaultSearchType.DEFAULT_SEARCH_MAIN.ordinal());
    }

    public void setSearchEngine(String selectedKeyword) {
        setSearchEngine(selectedKeyword, DefaultSearchType.DEFAULT_SEARCH_MAIN);
    }

    public void setSearchEngine(String selectedKeyword, DefaultSearchType type) {
        ThreadUtils.assertOnUiThread();
        TemplateUrlServiceJni.get()
                .setUserSelectedDefaultSearchProvider(
                        mNativeTemplateUrlServiceAndroid,
                        TemplateUrlService.this,
                        selectedKeyword,
                        ChoiceMadeLocation.OTHER,
                        type.ordinal());
    }

    /**
     * @return Whether the default search engine is managed and controlled by policy.  If true, the
     *         DSE can not be modified by the user.
     */
    public boolean isDefaultSearchManaged() {
        return TemplateUrlServiceJni.get()
                .isDefaultSearchManaged(mNativeTemplateUrlServiceAndroid, TemplateUrlService.this);
    }

    /** @return Whether or not the default search engine has search by image support. */
    public boolean isSearchByImageAvailable() {
        ThreadUtils.assertOnUiThread();
        return TemplateUrlServiceJni.get()
                .isSearchByImageAvailable(
                        mNativeTemplateUrlServiceAndroid, TemplateUrlService.this);
    }

    /**
     * @return Whether or not the default search engine has a logo image to show
     *    on NTP or start surface.
     */
    public boolean doesDefaultSearchEngineHaveLogo() {
        return TemplateUrlServiceJni.get()
                .doesDefaultSearchEngineHaveLogo(
                        mNativeTemplateUrlServiceAndroid, TemplateUrlService.this);
    }

    /** @return Whether the default configured search engine is for a Google property. */
    public boolean isDefaultSearchEngineGoogle() {
        return TemplateUrlServiceJni.get()
                .isDefaultSearchEngineGoogle(
                        mNativeTemplateUrlServiceAndroid, TemplateUrlService.this);
    }

    /**
     * Checks whether a search result page is from a default search provider.
     * @param url The url for the search result page.
     * @return Whether the search result page with the given url from the default search provider.
     */
    public boolean isSearchResultsPageFromDefaultSearchProvider(GURL url) {
        ThreadUtils.assertOnUiThread();
        return TemplateUrlServiceJni.get()
                .isSearchResultsPageFromDefaultSearchProvider(
                        mNativeTemplateUrlServiceAndroid, TemplateUrlService.this, url);
    }

    /**
     * Registers a listener for the callback that indicates that the
     * TemplateURLService has loaded.
     */
    public void registerLoadListener(final LoadListener listener) {
        ThreadUtils.assertOnUiThread();
        boolean added = mLoadListeners.addObserver(listener);
        assert added;

        // If the load has already been completed, post a load complete to the observer.  Done
        // as an asynchronous call to keep the client code predictable in the loaded/unloaded state.
        if (isLoaded()) {
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        if (!mLoadListeners.hasObserver(listener)) return;

                        listener.onTemplateUrlServiceLoaded();
                    });
        }
    }

    /**
     * Unregisters a listener for the callback that indicates that the
     * TemplateURLService has loaded.
     */
    public void unregisterLoadListener(LoadListener listener) {
        ThreadUtils.assertOnUiThread();
        boolean removed = mLoadListeners.removeObserver(listener);
        assert removed;
    }

    /**
     * Adds an observer to be notified on changes to the template URLs.
     * @param observer The observer to be added.
     */
    public void addObserver(TemplateUrlServiceObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer for changes to the template URLs.
     * @param observer The observer to be removed.
     */
    public void removeObserver(TemplateUrlServiceObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Finds the default search engine for the default provider and returns the url query
     * {@link String} for {@code query}.
     * @param query The {@link String} that represents the text query the search url should
     *              represent.
     * @return      A {@link String} that contains the url of the default search engine with
     *              {@code query} inserted as the search parameter.
     */
    public String getUrlForSearchQuery(String query) {
        return getUrlForSearchQuery(query, null, DefaultSearchType.DEFAULT_SEARCH_MAIN);
    }

    public String getUrlForSearchQuery(String query, PostParams postParams, DefaultSearchType type) {
        return getUrlForSearchQuery(query, null, postParams, type);
    }

    /**
     * Finds the default search engine for the default provider and returns the url query
     * {@link String} for {@code query}.
     * @param query The {@link String} that represents the text query the search url should
     *              represent.
     * @param searchParams A list of search params to be appended to the query.
     * @return      A {@link String} that contains the url of the default search engine with
     *              {@code query} inserted as the search parameter.
     */
    public String getUrlForSearchQuery(String query, List<String> searchParams) {
        return getUrlForSearchQuery(query, searchParams, null, DefaultSearchType.DEFAULT_SEARCH_MAIN);
    }

    public String getUrlForSearchQuery(String query, List<String> searchParams, PostParams postParams, DefaultSearchType type) {
        return TemplateUrlServiceJni.get()
                .getUrlForSearchQuery(
                        mNativeTemplateUrlServiceAndroid,
                        TemplateUrlService.this,
                        query,
                        searchParams == null ? null : searchParams.toArray(new String[0]), postParams, type.ordinal());
    }

    /**
     * See {@link #getSearchQueryForUrl(GURL)}.
     *
     * @deprecated Please use {@link #getSearchQueryForUrl(GURL)} instead.
     */
    @Deprecated
    public String getSearchQueryForUrl(String url) {
        return getSearchQueryForUrl(new GURL(url));
    }

    /** Finds the query in the url, if any. Returns empty if no query is present. */
    public String getSearchQueryForUrl(GURL url) {
        return TemplateUrlServiceJni.get()
                .getSearchQueryForUrl(
                        mNativeTemplateUrlServiceAndroid, TemplateUrlService.this, url);
    }

    /**
     * Finds the default search engine for the default provider and returns the url query
     * {@link String} for {@code query} with voice input source param set.
     * @param query The {@link String} that represents the text query the search url should
     *              represent.
     * @return      A {@link String} that contains the url of the default search engine with
     *              {@code query} inserted as the search parameter and voice input source param set.
     */
    public GURL getUrlForVoiceSearchQuery(String query) {
        return TemplateUrlServiceJni.get()
                .getUrlForVoiceSearchQuery(
                        mNativeTemplateUrlServiceAndroid, TemplateUrlService.this, query);
    }

    /**
     * Finds the default search engine for the default provider and returns the url query
     * {@link String} for {@code query} with the contextual search version param set.
     * @param query The search term to use as the main query in the returned search url.
     * @param alternateTerm The alternate search term to use as an alternate suggestion.
     * @param shouldPrefetch Whether the returned url should include a prefetch parameter.
     * @param protocolVersion The version of the Contextual Search API protocol to use.
     * @return      A {@link String} that contains the url of the default search engine with
     *              {@code query} and {@code alternateTerm} inserted as parameters and contextual
     *              search and prefetch parameters conditionally set.
     */
    public GURL getUrlForContextualSearchQuery(
            String query, String alternateTerm, boolean shouldPrefetch, String protocolVersion) {
        return TemplateUrlServiceJni.get()
                .getUrlForContextualSearchQuery(
                        mNativeTemplateUrlServiceAndroid,
                        TemplateUrlService.this,
                        query,
                        alternateTerm,
                        shouldPrefetch,
                        protocolVersion);
    }

    /**
     * Finds the URL for the search engine for the given keyword.
     * @param keyword The templateUrl keyword to look up.
     * @return      A {@link String} that contains the url of the specified search engine.
     */
    public String getSearchEngineUrlFromTemplateUrl(String keyword) {
        return TemplateUrlServiceJni.get()
                .getSearchEngineUrlFromTemplateUrl(
                        mNativeTemplateUrlServiceAndroid, TemplateUrlService.this, keyword);
    }

    /**
     * Finds the search engine type for the given keyword.
     * @param keyword The templateUrl keyword to look up.
     * @return      The search engine type of the specified search engine that contains the keyword.
     */
    public int getSearchEngineTypeFromTemplateUrl(String keyword) {
        return TemplateUrlServiceJni.get()
                .getSearchEngineTypeFromTemplateUrl(
                        mNativeTemplateUrlServiceAndroid, TemplateUrlService.this, keyword);
    }

    /**
     * Adds a search engine, set by Play API.
     *
     * @param name The name of the search engine to be added.
     * @param keyword The keyword of the search engine to be added.
     * @param searchUrl Search url of the search engine to be added.
     * @param suggestUrl Url for retrieving search suggestions.
     * @param faviconUrl Favicon url of the search engine to be added.
     * @param newTabUrl Url to be shown on a new tab.
     * @param imageUrl Url for reverse image search.
     * @param imageUrlPostParams The POST param name to use for the image payload transmitted.
     * @param imageTranslateUrl Url for image translation.
     * @param imageTranslateSourceLanguageParamKey Key of the url parameter identifying the source
     *     language for an image translation.
     * @param imageTranslateTargetLanguageParamKey Key of the url parameter identifying the target
     *     language for an image translation.
     * @return True if search engine was successfully added, false if search engine from Play API
     *     with such keyword already existed (e.g. from previous attempt to set search engine).
     */
    public boolean setPlayAPISearchEngine(
            String name,
            String keyword,
            String searchUrl,
            String suggestUrl,
            String faviconUrl,
            String newTabUrl,
            String imageUrl,
            String imageUrlPostParams,
            String imageTranslateUrl,
            String imageTranslateSourceLanguageParamKey,
            String imageTranslateTargetLanguageParamKey) {
        return TemplateUrlServiceJni.get()
                .setPlayAPISearchEngine(
                        mNativeTemplateUrlServiceAndroid,
                        TemplateUrlService.this,
                        name,
                        keyword,
                        searchUrl,
                        suggestUrl,
                        faviconUrl,
                        newTabUrl,
                        imageUrl,
                        imageUrlPostParams,
                        imageTranslateUrl,
                        imageTranslateSourceLanguageParamKey,
                        imageTranslateTargetLanguageParamKey);
    }

    public String addSearchEngineForTesting(String keyword, int ageInDays) {
        return TemplateUrlServiceJni.get()
                .addSearchEngineForTesting(
                        mNativeTemplateUrlServiceAndroid,
                        TemplateUrlService.this,
                        keyword,
                        ageInDays);
    }

    /**
     * Finds the image search url for the search engine used and the post content type for the
     * image.
     * @return An array of size 2 with the first element being the image search url, the second
     *         being the post content type.
     */
    public String[] getImageUrlAndPostContent() {
        return TemplateUrlServiceJni.get()
                .getImageUrlAndPostContent(
                        mNativeTemplateUrlServiceAndroid, TemplateUrlService.this);
    }

    /**
     * Whether the device is from an EEA country. This is consistent with countries which are
     * eligible for the EEA default search engine choice prompt. "Default country: or "country at
     * install" are used for SearchEngineChoiceCountry. It might be different than what LocaleUtils
     * returns.
     */
    public boolean isEeaChoiceCountry() {
        return TemplateUrlServiceJni.get().isEeaChoiceCountry(mNativeTemplateUrlServiceAndroid);
    }

    @NativeMethods
    public interface Natives {
        void load(long nativeTemplateUrlServiceAndroid, TemplateUrlService caller);

        boolean isLoaded(long nativeTemplateUrlServiceAndroid, TemplateUrlService caller);

        void setUserSelectedDefaultSearchProvider(
                long nativeTemplateUrlServiceAndroid,
                TemplateUrlService caller,
                String selectedKeyword,
                int choiceLocation,
                int type);

        boolean isDefaultSearchManaged(
                long nativeTemplateUrlServiceAndroid, TemplateUrlService caller);

        boolean isSearchResultsPageFromDefaultSearchProvider(
                long nativeTemplateUrlServiceAndroid, TemplateUrlService caller, GURL url);

        boolean isSearchByImageAvailable(
                long nativeTemplateUrlServiceAndroid, TemplateUrlService caller);

        boolean doesDefaultSearchEngineHaveLogo(
                long nativeTemplateUrlServiceAndroid, TemplateUrlService caller);

        boolean isDefaultSearchEngineGoogle(
                long nativeTemplateUrlServiceAndroid, TemplateUrlService caller);

        String getUrlForSearchQuery(
                long nativeTemplateUrlServiceAndroid,
                TemplateUrlService caller,
                String query,
                String[] searchParams,
                PostParams postParams,
                int type);

        String getSearchQueryForUrl(
                long nativeTemplateUrlServiceAndroid, TemplateUrlService caller, GURL url);

        GURL getUrlForVoiceSearchQuery(
                long nativeTemplateUrlServiceAndroid, TemplateUrlService caller, String query);

        GURL getUrlForContextualSearchQuery(
                long nativeTemplateUrlServiceAndroid,
                TemplateUrlService caller,
                String query,
                String alternateTerm,
                boolean shouldPrefetch,
                String protocolVersion);

        String getSearchEngineUrlFromTemplateUrl(
                long nativeTemplateUrlServiceAndroid, TemplateUrlService caller, String keyword);

        int getSearchEngineTypeFromTemplateUrl(
                long nativeTemplateUrlServiceAndroid, TemplateUrlService caller, String keyword);

        String addSearchEngineForTesting(
                long nativeTemplateUrlServiceAndroid,
                TemplateUrlService caller,
                String keyword,
                int offset);

        boolean setPlayAPISearchEngine(
                long nativeTemplateUrlServiceAndroid,
                TemplateUrlService caller,
                String name,
                String keyword,
                String searchUrl,
                String suggestUrl,
                String faviconUrl,
                String newTabUrl,
                String imageUrl,
                String imageUrlPostParams,
                String imageTranslateUrl,
                String imageTranslateSourceLanguageParamKey,
                String imageTranslateTargetLanguageParamKey);

        void getTemplateUrls(
                long nativeTemplateUrlServiceAndroid,
                TemplateUrlService caller,
                List<TemplateUrl> templateUrls);

        TemplateUrl getDefaultSearchEngine(
                long nativeTemplateUrlServiceAndroid, TemplateUrlService caller);

        String[] getImageUrlAndPostContent(
                long nativeTemplateUrlServiceAndroid, TemplateUrlService caller);

        boolean isEeaChoiceCountry(long nativeTemplateUrlServiceAndroid);

        void vivaldiSetDefaultOverride(long nativeTemplateUrlServiceAndroid,
                TemplateUrlService caller, String selectedKeyword);
        void vivaldiResetDefaultOverride(long nativeTemplateUrlServiceAndroid,
                TemplateUrlService caller);
        TemplateUrl vivaldiGetDefaultSearchEngine(long nativeTemplateUrlServiceAndroid,
                                                  TemplateUrlService caller, int type);
    }

    public void vivaldiSetSearchEngineOverride(String selectedKeyword) {
        ThreadUtils.assertOnUiThread();
        TemplateUrlServiceJni.get().vivaldiSetDefaultOverride(
                mNativeTemplateUrlServiceAndroid, TemplateUrlService.this, selectedKeyword);
    }

    public void vivaldiResetSearchEngineOverride() {
        ThreadUtils.assertOnUiThread();
        TemplateUrlServiceJni.get().vivaldiResetDefaultOverride(
                mNativeTemplateUrlServiceAndroid, TemplateUrlService.this);
    }

    public @Nullable TemplateUrl vivaldiGetDefaultSearchEngine(DefaultSearchType type) {
        if (!isLoaded()) return null;
        return TemplateUrlServiceJni.get().vivaldiGetDefaultSearchEngine(
                mNativeTemplateUrlServiceAndroid, TemplateUrlService.this, type.ordinal());
    }

    @CalledByNative
    private static void PopulatePostParams(
            PostParams postParams, String contentType, byte[] postContent) {
        postParams.contentType = contentType;
        postParams.postContent = postContent;
    }
    // End Vivaldi
}
