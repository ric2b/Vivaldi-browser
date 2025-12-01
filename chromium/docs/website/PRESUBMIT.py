# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Top-level presubmit script for the Git repo backing chromium.org.

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import re
from typing import Iterator, List, NamedTuple, Tuple
import urllib.parse

PRESUBMIT_VERSION = '2.0.0'

# This line is 'magic' in that git-cl looks for it to decide whether to
# use Python3 instead of Python2 when running the code in this file.
USE_PYTHON3 = True


def get_lob_extensions(input_api) -> Iterator[str]:
    """Get the list of known LOB extensions."""
    path = input_api.os_path.join(input_api.PresubmitLocalPath(), 'site',
                                  '.gitignore')
    ilines = input_api.ReadFile(path).splitlines()
    for line in ilines:
        if line != '# start_lob_ignore':
            continue

        for line in ilines:
            if line == '# end_lob_ignore':
                return

            if line and not line.startswith('#'):
                assert line.startswith('*')
                yield line[1:]


def CheckPatchFormatted(input_api, output_api):
    return input_api.canned_checks.CheckPatchFormatted(input_api, output_api)


def CheckChangeHasDescription(input_api, output_api):
    return input_api.canned_checks.CheckChangeHasDescription(
        input_api, output_api)


def CheckForLobs(input_api, output_api):
    output_status = []
    lob_extensions = list(get_lob_extensions(input_api))
    for file in input_api.change.AffectedFiles():
        # The tar.gz for example prevents using a hashmap to look up the
        # extension.
        for ext in lob_extensions:
            if str(file).endswith(ext) and file.Action() != 'D':
                error_msg = (
                    'The file \'{file_name}\' is a binary that has not been '
                    'uploaded to GCE. Please run:\n\tscripts/upload_lobs.py '
                    '"{file_name}"\nand commit {file_name}.sha1 instead\n'
                    'Run:\n\tgit rm --cached "{file_name}"\n'
                    'to remove the lob from git'.format(
                        file_name=file.LocalPath()))

                error = output_api.PresubmitError(error_msg)
                output_status.append(error)
                break

    return output_status


def CheckPatchFormatted(input_api, output_api):
    """Check formatting of files."""
    return input_api.canned_checks.CheckPatchFormatted(input_api, output_api)


class _MdLink(NamedTuple):
    """Link found in markdown."""

    # The file link is found in.
    file: str

    # The file has just been created.
    new: bool

    # The visible link text.
    text: str

    # The actual link.
    uri: str

    # Whether the link supports local/relative paths like /dir/foo.md.
    relative_ok: bool

    # What line was the link found on?
    line_num: int


# Mapping of preferred host names.  If we find people using <key>, we'll
# make them use <value> instead.
_MD_HOST_ALIASES = {
    # keep-sorted start
    'b': 'issuetracker.google.com',
    'chromium.org': 'www.chromium.org',
    'dev.chromium.org': 'www.chromium.org',
    'goto': 'go',
    'goto.google.com': 'go',
    'www.youtube.com': 'youtube.com',
    # keep-sorted end
}

# These hosts should always use https://
# This isn't an exhaustive list, just hosts we commonly refer to.
# TODO(vapier): Require https:// on all hosts by default, and require any
# actual http:// hosts be enumerated below.  This requires a large cleanup
# of existing docs first.
_MD_HTTPS_HOSTS = {
    # keep-sorted start
    'crbug.com',
    'crrev.com',
    'en.wikipedia.org',
    'github.com',
    'google.com',
    'issuetracker.google.com',
    'www.chromium.org',
    'www.google.com',
    'www.w3.org',
    'youtu.be',
    'youtube.com',
    # keep-sorted end
}

# These hosts should always use http://
_MD_HTTP_HOSTS = {
    # keep-sorted start
    'g',
    'go',
    # keep-sorted end
}


def _extract_markdown_links(results, input_api, output_api,
                            affected_file) -> List[_MdLink]:
    """Walk `affected_file` and extract links.

    This parser is not complete relative to the spec our website supports, but
    it covers pretty much all the actual usage we have.

    We have a variety of styles:
      [text](link)
      [anchor]: link
      [anchor]: link "extra text"
      [^footnote]
      [^footnote]: content
      <link>
      link
    """
    # Extract all the content that links will be rendered in.  Basically,
    # filter out all the code blocks where content is not rendered.
    file = affected_file.LocalPath()
    new_file = affected_file.Action() == 'A'
    line_gen = enumerate(affected_file.NewContents(), start=1)
    filtered_lines = []
    for i, line in line_gen:
        # Ignore ``` blocks because the contents are not markdown, and they
        # might use code that matches the link syntax (e.g. regexes).
        # TODO: <pre><code> blocks.
        # TODO: `...` inline text -- but outside of [text].
        # TODO: 4 space automatic indented blocks.
        sline = line.strip()
        # Handle things like:
        # ```
        # ```language
        # ```this is triple ` quotes inline```
        if sline.startswith('```') and (len(sline) == 3
                                        or not sline.endswith('```')):
            while True:
                # Add stub lines to keep line numbers accurate.
                filtered_lines.append('')
                try:
                    _, line = next(line_gen)
                except StopIteration:
                    results.append(
                        output_api.PresubmitError(
                            f'{file}:{i}: Missing closing ``` blocks'))
                    line = '```'
                if line.lstrip().startswith('```'):
                    break

        filtered_lines.append(line)

    content = '\n'.join(filtered_lines)

    def _match_to_details(m) -> Tuple[str, str, int]:
        """Find line number for match object."""
        text = ' '.join(m.group(1).strip().split())
        link = m.group(2)
        # This is not efficient, but seems to be simple & fast enough to not
        # warrant an intelligent algorithm.
        linenum = content[0:m.start()].count('\n') + 1
        return (text, link, linenum)

    ret = []

    # [text](link)
    # While [text] may span multiple lines, (link) may not.
    for m in re.finditer(r'\[([^]]+)\]\(([^)\s]+)\)', content, flags=re.M):
        text, link, linenum = _match_to_details(m)
        ret += [_MdLink(file, new_file, text, link, True, linenum)]

    # [anchor]: link
    for m in re.finditer(r'^\[([^]]+)\]:(?: *)(\S+)', content, flags=re.M):
        text, link, linenum = _match_to_details(m)
        # [^footnote]: link
        if not text.startswith('^'):
            ret += [_MdLink(file, new_file, text, link, True, linenum)]

    # <link>
    for m in re.finditer(r'<((https?://[^>]+))>', content, flags=re.M):
        text, link, linenum = _match_to_details(m)
        ret += [_MdLink(file, new_file, text, link, False, linenum)]

    return ret


def CheckLinks(input_api, output_api):
    """Check links used in markdown."""
    # Build up the files to analyze.
    affected_files = input_api.AffectedFiles(
        file_filter=lambda x: x.LocalPath().endswith('.md'))

    results = []

    # Extract the links from the files.
    links = []
    for affected_file in affected_files:
        links += _extract_markdown_links(results, input_api, output_api,
                                         affected_file)

    # Check links.

    def _create_result(link, msg, want_uri) -> None:
        want_link = urllib.parse.urlunparse(want_uri)
        results.append(
            output_api.PresubmitError(f'{link.file}:{link.line_num}: {msg}',
                                      long_text=f'- {link.uri}\n+ {want_link}'))

    for link in links:
        o = urllib.parse.urlparse(link.uri)

        # Check bad http:// usage.
        if o.scheme == 'http' and o.netloc in _MD_HTTPS_HOSTS:
            _create_result(link, 'Always use https:// with this host',
                           o._replace(scheme='https'))

        # Check bad https:// usage.
        if o.scheme == 'https' and o.netloc in _MD_HTTP_HOSTS:
            _create_result(link, 'Always use http:// with this host',
                           o._replace(scheme='http'))

        # Check host aliases.
        for oldhost, newhost in _MD_HOST_ALIASES.items():
            if o.netloc == oldhost:
                _create_result(link, f'Use {newhost} in links',
                               o._replace(netloc=newhost))

        # Have people use relative /foo/bar links instead of
        # https//www.chromium.org/foo/bar so we can check target links, and so
        # navigating via the sandbox website works correctly.
        if (link.relative_ok and o.netloc == 'www.chromium.org'
                and link.file.startswith('site/')):
            _create_result(
                link, 'Use local paths instead of www.chromium.org in links',
                o._replace(scheme='', netloc='', path=o.path or '/'))

        # People shouldn't link to chromium.org markdown pages via gitiles.
        if o.netloc == 'chromium.googlesource.com' and o.path.startswith(
                '/website/+/HEAD/site/'):
            path = o.path[20:]
            if path.endswith('/index.md'):
                path = path[:-9]
            _create_result(
                link, 'Use local paths instead of chromium.googlesource.com',
                o._replace(scheme='', netloc='', path=path))

        # Check relative links for generated docs (under site/).
        if o.scheme == o.netloc == '' and link.file.startswith('site/'):
            # Relative links to markdown files don't work in generated pages.
            if (o.path.startswith('/')
                    or o.path.startswith('.')) and o.path.endswith('.md'):
                _create_result(
                    link, 'Do not link directly to markdown files',
                    o._replace(path='/'.join(o.path.split('/')[:-1])))

            # The /site/ prefix is removed in generated content, but works when
            # viewing under gitiles, so sometimes people test the wrong page.
            if o.path.startswith('/site/'):
                _create_result(link, 'Omit the /site/ prefix in local paths',
                               o._replace(path=o.path[5:]))

            # Verify local paths exist.
            if o.path:
                # Anchor absolute paths under site/, otherwise it's relative to
                # the document.
                local_path = input_api.os_path.join(
                    'site' if o.path.startswith('/') else
                    input_api.os_path.dirname(link.file),
                    urllib.parse.unquote(o.path.lstrip('/')))

                if o.path == '/system/errors/NodeNotFound' and not link.new:
                    results.append(
                        output_api.PresubmitPromptWarning(
                            f'{link.file}:{link.line_num}: '
                            f'Missing link: {o.path}'))

                # Links can point to:
                # * Directory (with implicit /index.md).
                #   /foo/bar points to /foo/bar/index.md
                # * LOB file (e.g. images).
                #   /foo/bar.png has /foo/bar.png.sha1
                # * Raw file (e.g. html); does not support .md files.
                #   /foo/bar.html
                # Links can also point to specific md files, but we don't use
                # that style, so don't support it.  We prefer to use dirs and
                # index.md files exclusively.
                elif not (input_api.os_path.exists(
                        input_api.os_path.join(local_path, 'index.md'))
                          or input_api.os_path.exists(local_path + '.sha1')
                          or input_api.os_path.isfile(local_path)):
                    # TODO(vapier): Make these fatal.
                    if '.' not in local_path and not link.new:
                        results.append(
                            output_api.PresubmitPromptWarning(
                                f'{link.file}:{link.line_num}: '
                                f'Missing link: {o.path}'))
                    else:
                        _create_result(link, 'Link appears to be broken',
                                       o._replace(path='???', fragment=''))

    return results
