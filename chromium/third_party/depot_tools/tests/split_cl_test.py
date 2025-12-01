#!/usr/bin/env vpython3
"""Tests for split_cl."""

import os
import sys
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import split_cl
import gclient_utils


class SplitClTest(unittest.TestCase):

    @property
    def _input_dir(self):
        base = os.path.splitext(os.path.abspath(__file__))[0]
        # Here _testMethodName is a string like "testCmdAssemblyFound"
        # If the test doesn't have its own subdirectory, it uses a common one
        path = os.path.join(base + ".inputs", self._testMethodName)
        if not os.path.isdir(path):
            path = os.path.join(base + ".inputs", "commonFiles")
        return path

    def testAddUploadedByGitClSplitToDescription(self):
        description = """Convert use of X to Y in $description

<add some background about this conversion for the reviewers>

"""
        footers = 'Bug: 12345'

        added_line = 'This CL was uploaded by git cl split.'
        experimental_lines = ("This CL was uploaded by an experimental version "
                              "of git cl split\n"
                              "(https://crbug.com/389069356).")

        # Description without footers
        self.assertEqual(
            split_cl.AddUploadedByGitClSplitToDescription(description),
            description + added_line)

        # Description with footers
        self.assertEqual(
            split_cl.AddUploadedByGitClSplitToDescription(description +
                                                          footers),
            description + added_line + '\n\n' + footers)

        # Description with footers and experimental flag
        self.assertEqual(
            split_cl.AddUploadedByGitClSplitToDescription(
                description + footers, True),
            description + experimental_lines + '\n\n' + footers)

    @mock.patch("split_cl.EmitWarning")
    def testFormatDescriptionOrComment(self, mock_emit_warning):
        description = "Converted use of X to Y in $description."

        # One directory
        self.assertEqual(
            split_cl.FormatDescriptionOrComment(
                description, split_cl.FormatDirectoriesForPrinting(["foo"])),
            "Converted use of X to Y in foo.",
        )

        # Many directories
        self.assertEqual(
            split_cl.FormatDescriptionOrComment(
                description,
                split_cl.FormatDirectoriesForPrinting(["foo", "bar"])),
            "Converted use of X to Y in ['foo', 'bar'].",
        )

        mock_emit_warning.assert_not_called()

        description_deprecated = "Converted use of X to Y in $directory."

        # Make sure we emit a deprecation warning if the old format is used
        self.assertEqual(
            split_cl.FormatDescriptionOrComment(
                description_deprecated,
                split_cl.FormatDirectoriesForPrinting([])),
            "Converted use of X to Y in [].",
        )

        mock_emit_warning.assert_called_once()

    def GetDirectoryBaseName(self, file_path):
        return os.path.basename(os.path.dirname(file_path))

    def MockSuggestOwners(self, paths, exclude=None):
        if not paths:
            return ["superowner"]
        return self.GetDirectoryBaseName(paths[0]).split(",")

    def MockIsFile(self, file_path):
        if os.path.basename(file_path) == "OWNERS":
            return "owner" in self.GetDirectoryBaseName(file_path)

        return True

    @mock.patch("os.path.isfile")
    def testSelectReviewersForFiles(self, mock_is_file):
        mock_is_file.side_effect = self.MockIsFile
        owners_client = mock.Mock(SuggestOwners=self.MockSuggestOwners,
                                  EVERYONE="*")
        cl = mock.Mock(owners_client=owners_client)

        files = [("M", os.path.join("foo", "owner1,owner2", "a.txt")),
                 ("M", os.path.join("foo", "owner1,owner2", "b.txt")),
                 ("M", os.path.join("bar", "owner1,owner2", "c.txt")),
                 ("M", os.path.join("bax", "owner2", "d.txt")),
                 ("M", os.path.join("baz", "owner3", "e.txt"))]

        files_split_by_reviewers = split_cl.SelectReviewersForFiles(
            cl, "author", files, 0, "")

        self.assertEqual(3, len(files_split_by_reviewers.keys()))
        info1 = files_split_by_reviewers[tuple(["owner1", "owner2"])]
        self.assertEqual(info1.files,
                         [("M", os.path.join("foo", "owner1,owner2", "a.txt")),
                          ("M", os.path.join("foo", "owner1,owner2", "b.txt")),
                          ("M", os.path.join("bar", "owner1,owner2", "c.txt"))])
        self.assertEqual(info1.owners_directories,
                         ["foo/owner1,owner2", "bar/owner1,owner2"])
        info2 = files_split_by_reviewers[tuple(["owner2"])]
        self.assertEqual(info2.files,
                         [("M", os.path.join("bax", "owner2", "d.txt"))])
        self.assertEqual(info2.owners_directories, ["bax/owner2"])
        info3 = files_split_by_reviewers[tuple(["owner3"])]
        self.assertEqual(info3.files,
                         [("M", os.path.join("baz", "owner3", "e.txt"))])
        self.assertEqual(info3.owners_directories, ["baz/owner3"])

    class UploadClTester:
        """Sets up test environment for testing split_cl.UploadCl()"""

        def __init__(self, test):
            self.mock_git_branches = self.StartPatcher("git_common.branches",
                                                       test)
            self.mock_git_branches.return_value = []
            self.mock_git_current_branch = self.StartPatcher(
                "git_common.current_branch", test)
            self.mock_git_current_branch.return_value = "branch_to_upload"
            self.mock_git_run = self.StartPatcher("git_common.run", test)
            self.mock_temporary_file = self.StartPatcher(
                "gclient_utils.temporary_file", test)
            self.mock_temporary_file(
            ).__enter__.return_value = "temporary_file0"
            self.mock_file_writer = self.StartPatcher("gclient_utils.FileWrite",
                                                      test)

        def StartPatcher(self, target, test):
            patcher = mock.patch(target)
            test.addCleanup(patcher.stop)
            return patcher.start()

        def DoUploadCl(self, description, files, reviewers, cmd_upload):
            split_cl.UploadCl("branch_to_upload", "upstream_branch",
                              description, files, "description",
                              "splitting_file.txt", None, reviewers,
                              mock.Mock(), cmd_upload, True, True, "topic",
                              os.path.sep)

    def testUploadCl(self):
        """Tests commands run by UploadCl."""

        upload_cl_tester = self.UploadClTester(self)

        description = split_cl.FormatDirectoriesForPrinting(["dir0"])
        files = [("M", os.path.join("bar", "a.cc")),
                 ("D", os.path.join("foo", "b.cc"))]
        reviewers = {"reviewer1@gmail.com", "reviewer2@gmail.com"}
        mock_cmd_upload = mock.Mock()
        upload_cl_tester.DoUploadCl(description, files, reviewers,
                                    mock_cmd_upload)

        abs_repository_path = os.path.abspath(os.path.sep)
        mock_git_run = upload_cl_tester.mock_git_run
        self.assertEqual(mock_git_run.call_count, 4)
        mock_git_run.assert_has_calls([
            mock.call("checkout", "-t", "upstream_branch", "-b",
                      split_cl.CreateBranchName("branch_to_upload", files)),
            mock.call("rm", os.path.join(abs_repository_path, "foo", "b.cc")),
            mock.call("checkout", "branch_to_upload", "--",
                      os.path.join(abs_repository_path, "bar", "a.cc")),
            mock.call("commit", "-F", "temporary_file0")
        ])

        expected_upload_args = [
            "-f", "-r", "reviewer1@gmail.com,reviewer2@gmail.com",
            "--cq-dry-run", "--send-mail", "--enable-auto-submit",
            "--topic=topic"
        ]
        mock_cmd_upload.assert_called_once_with(expected_upload_args)

    def testDontUploadClIfBranchAlreadyExists(self):
        """Tests that a CL is not uploaded if split branch already exists"""

        upload_cl_tester = self.UploadClTester(self)

        description = split_cl.FormatDirectoriesForPrinting(["dir0"])
        files = [("M", os.path.join("bar", "a.cc")),
                 ("D", os.path.join("foo", "b.cc"))]
        reviewers = {"reviewer1@gmail.com"}
        mock_cmd_upload = mock.Mock()
        upload_cl_tester.mock_git_branches.return_value = [
            "branch0",
            split_cl.CreateBranchName("branch_to_upload", files)
        ]
        upload_cl_tester.DoUploadCl(description, files, reviewers,
                                    mock_cmd_upload)

        upload_cl_tester.mock_git_run.assert_not_called()
        mock_cmd_upload.assert_not_called()

    @mock.patch("gclient_utils.AskForData")
    def testCheckDescriptionBugLink(self, mock_ask_for_data):
        # Description contains bug link.
        self.assertTrue(split_cl.CheckDescriptionBugLink("Bug:1234"))
        self.assertEqual(mock_ask_for_data.call_count, 0)

        # Description does not contain bug link. User does not enter 'y' when
        # prompted.
        mock_ask_for_data.reset_mock()
        mock_ask_for_data.return_value = "m"
        self.assertFalse(split_cl.CheckDescriptionBugLink("Description"))
        self.assertEqual(mock_ask_for_data.call_count, 1)

        # Description does not contain bug link. User enters 'y' when prompted.
        mock_ask_for_data.reset_mock()
        mock_ask_for_data.return_value = "y"
        self.assertTrue(split_cl.CheckDescriptionBugLink("Description"))
        self.assertEqual(mock_ask_for_data.call_count, 1)

    @mock.patch("gclient_utils.FileRead", return_value="Description")
    def testLoadDescription(self, mock_file_read):
        # No description provided, use the dummy:
        self.assertTrue(
            split_cl.LoadDescription(None, True).startswith("Dummy"))
        self.assertEqual(mock_file_read.call_count, 0)

        # No description provided during a real run
        self.assertRaises(ValueError, split_cl.LoadDescription, None, False)
        self.assertEqual(mock_file_read.call_count, 0)

        # Description file provided, load it regardless of dry run
        self.assertEqual(split_cl.LoadDescription("SomeFile.txt", False),
                         "Description")
        self.assertEqual(mock_file_read.call_count, 1)

        mock_file_read.reset_mock()
        self.assertEqual(split_cl.LoadDescription("SomeFile.txt", True),
                         "Description")
        self.assertEqual(mock_file_read.call_count, 1)

    class SplitClTester:
        """Sets up test environment for testing split_cl.SplitCl()"""

        def __init__(self, test):
            self.mocks = []
            self.mock_file_read = self.StartPatcher(
                "gclient_utils.FileRead",
                test,
                return_value="Non-dummy description\nBug: 1243")
            self.mock_in_git_repo = self.StartPatcher(
                "split_cl.EnsureInGitRepository", test)
            self.mock_git_status = self.StartPatcher("scm.GIT.CaptureStatus",
                                                     test)
            self.mock_git_run = self.StartPatcher("git_common.run", test)
            self.mock_git_current_branch = self.StartPatcher(
                "git_common.current_branch",
                test,
                return_value="branch_to_upload")
            self.mock_git_branches = self.StartPatcher("git_common.branches",
                                                       test)
            self.mock_git_upstream = self.StartPatcher(
                "git_common.upstream", test, return_value="upstream_branch")
            self.mock_get_reviewers = self.StartPatcher(
                "split_cl.SelectReviewersForFiles", test)
            self.mock_ask_for_data = self.StartPatcher(
                "gclient_utils.AskForData", test)
            self.mock_print_cl_info = self.StartPatcher("split_cl.PrintClInfo",
                                                        test)
            self.mock_print_summary = self.StartPatcher("split_cl.PrintSummary",
                                                        test)
            self.mock_upload_cl = self.StartPatcher("split_cl.UploadCl", test)
            self.mock_save_splitting = self.StartPatcher(
                "split_cl.SaveSplittingToTempFile", test)
            # Suppress output for cleaner tests
            self.mock_emit = self.StartPatcher("split_cl.Emit", test)

        def StartPatcher(self, target, test, **kwargs):
            patcher = mock.patch(target, **kwargs)
            test.addCleanup(patcher.stop)
            m = patcher.start()
            self.mocks.append(m)
            return m

        def ResetMocks(self):
            for m in self.mocks:
                m.reset_mock()

        def DoSplitCl(self, description_file, dry_run, summarize,
                      reviewers_override, files_split_by_reviewers,
                      proceed_response):
            all_files = [v.files for v in files_split_by_reviewers.values()]
            all_files_flattened = [
                file for files in all_files for file in files
            ]

            self.mock_git_status.return_value = all_files_flattened
            self.mock_get_reviewers.return_value = files_split_by_reviewers
            self.mock_ask_for_data.return_value = proceed_response

            split_cl.SplitCl(description_file, None, mock.Mock(), mock.Mock(),
                             dry_run, summarize, reviewers_override, False,
                             False, None, None, None, None, None, None)

    # Save for re-use
    files_split_by_reviewers = {
        ("a@example.com", ):
        split_cl.FilesAndOwnersDirectory([
            ("M", "a/b/foo.cc"),
            ("M", "d/e/bar.h"),
        ], []),
        ("b@example.com", ):
        split_cl.FilesAndOwnersDirectory([
            ("A", "f/g/baz.py"),
        ], [])
    }

    def testSplitClConfirm(self):
        split_cl_tester = self.SplitClTester(self)

        # Should prompt for confirmation and upload several times
        split_cl_tester.DoSplitCl("SomeFile.txt", False, False, None,
                                  self.files_split_by_reviewers, "y")

        split_cl_tester.mock_ask_for_data.assert_called_once()
        split_cl_tester.mock_print_cl_info.assert_not_called()
        self.assertEqual(split_cl_tester.mock_upload_cl.call_count,
                         len(self.files_split_by_reviewers))

        split_cl_tester.ResetMocks()
        # Should prompt for confirmation and not upload
        split_cl_tester.DoSplitCl("SomeFile.txt", False, False, None,
                                  self.files_split_by_reviewers, "f")

        split_cl_tester.mock_ask_for_data.assert_called_once()
        split_cl_tester.mock_print_cl_info.assert_not_called()
        split_cl_tester.mock_upload_cl.assert_not_called()

        split_cl_tester.ResetMocks()

        # Dry runs: Don't prompt, print info instead of uploading
        split_cl_tester.DoSplitCl("SomeFile.txt", True, False, None,
                                  self.files_split_by_reviewers, "f")

        split_cl_tester.mock_ask_for_data.assert_not_called()
        self.assertEqual(split_cl_tester.mock_print_cl_info.call_count,
                         len(self.files_split_by_reviewers))
        split_cl_tester.mock_print_summary.assert_not_called()
        split_cl_tester.mock_upload_cl.assert_not_called()

        split_cl_tester.ResetMocks()
        # Summarize is true: Don't prompt, emit a summary
        split_cl_tester.DoSplitCl("SomeFile.txt", True, True, None,
                                  self.files_split_by_reviewers, "f")

        split_cl_tester.mock_ask_for_data.assert_not_called()
        split_cl_tester.mock_print_cl_info.assert_not_called()
        split_cl_tester.mock_print_summary.assert_called_once()
        split_cl_tester.mock_upload_cl.assert_not_called()

    def testReviewerOverride(self):
        split_cl_tester = self.SplitClTester(self)

        def testOneOverride(reviewers_lst):
            split_cl_tester.DoSplitCl("SomeFile.txt", False, False,
                                      reviewers_lst,
                                      self.files_split_by_reviewers, "y")

            for call in split_cl_tester.mock_upload_cl.call_args_list:
                self.assertEqual(call.args[7], set(reviewers_lst))

            split_cl_tester.ResetMocks()

        # The 'None' case gets ample testing everywhere else
        testOneOverride([])
        testOneOverride(['a@b.com', 'c@d.com'])

    def testValidateExistingBranches(self):
        """
        Make sure that we skip existing branches if they match what we intend
        to do, and fail if there are existing branches that don't match.
        """

        split_cl_tester = self.SplitClTester(self)

        # If no split branches exist, we should call upload once per CL
        split_cl_tester.mock_git_branches.return_value = [
            "branch0", "branch_to_upload"
        ]
        split_cl_tester.DoSplitCl("SomeFile.txt", False, False, None,
                                  self.files_split_by_reviewers, "y")
        self.assertEqual(split_cl_tester.mock_upload_cl.call_count,
                         len(self.files_split_by_reviewers))

        # TODO(389069356): We should also ensure that if there are existing
        # branches that match our current splitting, we skip them when uploading
        # Unfortunately, we're not set up to test that, so this will have to
        # wait until we've refactored SplitCl and UploadCL to be less
        # monolithic

        # If a split branch with a bad name already exists, we should fail
        split_cl_tester.mock_upload_cl.reset_mock()
        split_cl_tester.mock_git_branches.return_value = [
            "branch0", "branch_to_upload",
            "branch_to_upload_123456789_whatever_split"
        ]
        split_cl_tester.DoSplitCl("SomeFile.txt", False, False, None,
                                  self.files_split_by_reviewers, "y")
        split_cl_tester.mock_upload_cl.assert_not_called()


    # Tests related to saving to and loading from files
    # Sample CLInfos for testing
    CLInfo_1 = split_cl.CLInfo(reviewers=["a@example.com"],
                               description="['chrome/browser']",
                               files=[
                                   ("M", "chrome/browser/a.cc"),
                                   ("M", "chrome/browser/b.cc"),
                               ])

    CLInfo_2 = split_cl.CLInfo(reviewers=["a@example.com", "b@example.com"],
                               description="['foo', 'bar/baz']",
                               files=[("M", "foo/browser/a.cc"),
                                      ("M", "bar/baz/b.cc"),
                                      ("D", "foo/bar/c.h")])

    def testCLInfoFormat(self):
        """ Make sure CLInfo printing works as expected """

        def ReadAndStripPreamble(file):
            """ Read the contents of a file and strip the automatically-added
                preamble so we can do string comparison
            """
            content = gclient_utils.FileRead(os.path.join(
                self._input_dir, file))
            # Strip preamble
            stripped = [
                line for line in content.splitlines()
                if not line.startswith("#")
            ]
            # Strip newlines in preamble
            return "\n".join(stripped[2:])

        # Direct string comparison
        self.assertEqual(self.CLInfo_1.FormatForPrinting(),
                         ReadAndStripPreamble("1_cl.txt"))

        self.assertEqual(
            self.CLInfo_1.FormatForPrinting() +
            "\n\n" + self.CLInfo_2.FormatForPrinting(),
            ReadAndStripPreamble("2_cls.txt"))

    @mock.patch("split_cl.EmitWarning")
    def testParseCLInfo(self, mock_emit_warning):
        """ Make sure we can parse valid files """

        self.assertEqual([self.CLInfo_1],
                         split_cl.LoadSplittingFromFile(
                             os.path.join(self._input_dir, "1_cl.txt"),
                             self.CLInfo_1.files))
        self.assertEqual([self.CLInfo_1, self.CLInfo_2],
                         split_cl.LoadSplittingFromFile(
                             os.path.join(self._input_dir, "2_cls.txt"),
                             self.CLInfo_1.files + self.CLInfo_2.files))

        # Make sure everything in this file is valid to parse
        split_cl.LoadSplittingFromFile(
            os.path.join(self._input_dir, "odd_formatting.txt"),
            self.CLInfo_1.files + self.CLInfo_2.files + [("A", "a/b/c"),
                                                         ("A", "a/b/d"),
                                                         ("D", "a/e")])
        mock_emit_warning.assert_not_called()

    def testParseBadFiles(self):
        """ Make sure we don't parse invalid files """
        for file in os.listdir(self._input_dir):
            lines = gclient_utils.FileRead(os.path.join(self._input_dir,
                                                        file)).splitlines()
            self.assertRaises(split_cl.ClSplitParseError,
                              split_cl.ParseSplittings, lines)

    @mock.patch("split_cl.EmitWarning")
    @mock.patch("split_cl.Emit")
    def testValidateBadFiles(self, _, mock_emit_warning):
        """ Make sure we reject invalid CL lists """
        # Warn on an empty file
        split_cl.LoadSplittingFromFile(
            os.path.join(self._input_dir, "warn_0_cls.txt"), [])
        mock_emit_warning.assert_called_once()
        mock_emit_warning.reset_mock()

        # Warn if reviewers don't look like emails
        split_cl.LoadSplittingFromFile(
            os.path.join(self._input_dir, "warn_bad_reviewer_email.txt"),
            [("M", "a.cc")])
        self.assertEqual(mock_emit_warning.call_count, 2)
        mock_emit_warning.reset_mock()

        # Fail if a file appears in multiple CLs
        self.assertRaises(
            split_cl.ClSplitParseError, split_cl.LoadSplittingFromFile,
            os.path.join(self._input_dir, "error_file_in_multiple_cls.txt"),
            [("M", "chrome/browser/a.cc"), ("M", "chrome/browser/b.cc"),
             ("M", "bar/baz/b.cc"), ("D", "foo/bar/c.h")])

        # Fail if a file is listed that doesn't appear on disk
        self.assertRaises(
            split_cl.ClSplitParseError, split_cl.LoadSplittingFromFile,
            os.path.join(self._input_dir, "no_inherent_problems.txt"),
            [("M", "chrome/browser/a.cc"), ("M", "chrome/browser/b.cc")])
        self.assertRaises(
            split_cl.ClSplitParseError,
            split_cl.LoadSplittingFromFile,
            os.path.join(self._input_dir, "no_inherent_problems.txt"),
            [
                ("M", "chrome/browser/a.cc"),
                ("M", "chrome/browser/b.cc"),
                ("D", "c.h")  # Wrong action, should still error
            ])

        # Warn if not all files on disk are included
        split_cl.LoadSplittingFromFile(
            os.path.join(self._input_dir, "no_inherent_problems.txt"),
            [("M", "chrome/browser/a.cc"), ("M", "chrome/browser/b.cc"),
             ("A", "c.h"), ("D", "d.h")])
        mock_emit_warning.assert_called_once()

    @mock.patch("split_cl.Emit")
    @mock.patch("gclient_utils.FileWrite")
    def testParsingRoundTrip(self, mock_file_write, _):
        """ Make sure that if we parse a file and save the result,
            we get the same file. Only works on test files that are
            nicely formatted. """

        for file in os.listdir(self._input_dir):
            if file == "odd_formatting.txt":
                continue
            contents = gclient_utils.FileRead(
                os.path.join(self._input_dir, file))
            parsed_contents = split_cl.ParseSplittings(contents.splitlines())
            split_cl.SaveSplittingToFile(parsed_contents, "file.txt")

            written_lines = [
                args[0][1] for args in mock_file_write.call_args_list
            ]

            self.assertEqual(contents, "".join(written_lines))
            mock_file_write.reset_mock()

    @mock.patch("os.path.isfile", return_value=False)
    def testDirectoryTrie(self, _):
        """
        Simple unit tests for creating and reading from a DirectoryTrie.
        """
        # The trie code uses OS paths so we need to do the same here
        path_abc = os.path.join("a", "b", "c.cc")
        path_abd = os.path.join("a", "b", "d.h")
        path_aefgh = os.path.join("a", "e", "f", "g", "h.hpp")
        path_ijk = os.path.join("i", "j", "k.cc")
        path_al = os.path.join("a", "l.cpp")
        path_top = os.path.join("top.gn")

        files = [path_abc, path_abd, path_aefgh, path_ijk, path_al, path_top]
        split_files = [file.split(os.path.sep) for file in files]

        trie = split_cl.DirectoryTrie(False)
        trie.AddFiles(split_files)

        self.assertEqual(trie.files, [path_top])
        self.assertEqual(trie.subdirectories["a"].files, [path_al])
        self.assertEqual(trie.subdirectories["a"].subdirectories["b"].files,
                         [path_abc, path_abd])
        self.assertEqual(sorted(trie.ToList()), sorted(files))

        self.assertFalse(trie.has_parent)
        self.assertFalse(trie.subdirectories["a"].has_parent)
        self.assertTrue(trie.subdirectories["a"].subdirectories["b"].has_parent)

        self.assertEqual(trie.prefix, "")
        self.assertEqual(trie.subdirectories["a"].prefix, "a")
        self.assertEqual(trie.subdirectories["a"].subdirectories["b"].prefix,
                         os.path.join("a", "b"))

    @mock.patch("os.path.isfile", return_value=False)
    def testClusterFiles(self, _):
        """
        Make sure ClusterFiles returns sensible results for some sample inputs.
        """

        def compareClusterOutput(clusters: list[split_cl.Bin],
                                 file_groups: list[list[str]]):
            """
            Ensure that ClusterFiles grouped files the way we expected it to.
            """
            clustered_files = sorted([sorted(bin.files) for bin in clusters])
            file_groups = sorted([sorted(grp) for grp in file_groups])
            self.assertEqual(clustered_files, file_groups)

        # The clustering code uses OS paths so we need to do the same here
        path_abc = os.path.join("a", "b", "c.cc")
        path_abd = os.path.join("a", "b", "d.h")
        path_aefgh = os.path.join("a", "e", "f", "g", "h.hpp")
        path_ijk = os.path.join("i", "j", "k.cc")
        path_ilm = os.path.join("i", "l", "m.cc")
        path_an = os.path.join("a", "n.cpp")
        path_top = os.path.join("top.gn")
        files = [
            path_abc, path_abd, path_aefgh, path_ijk, path_ilm, path_an,
            path_top
        ]

        def checkClustering(min_files, max_files, expected):
            clusters = split_cl.ClusterFiles(False, files, min_files, max_files)
            compareClusterOutput(clusters, expected)

        # Each file gets its own cluster
        individual_files = [[file] for file in files]
        checkClustering(1, 1, individual_files)

        # Put both entries of a/b in the same cluster, everything else alone
        ab_together = [[path_abc, path_abd], [path_aefgh], [path_ijk],
                       [path_ilm], [path_an], [path_top]]
        checkClustering(1, 2, ab_together)
        checkClustering(1, 100, ab_together)

        # Groups of 2: a/b, rest of a/, all of i/.
        a_two_groups = [[path_abc, path_abd], [path_aefgh, path_an],
                        [path_ijk, path_ilm], [path_top]]
        checkClustering(2, 2, a_two_groups)
        checkClustering(3, 3, a_two_groups)

        # Put all of a/ together and all of i/ together.
        # Don't combine top-level directories with things at the root
        by_top_level_dir = [[path_abc, path_abd, path_aefgh, path_an],
                            [path_ijk, path_ilm], [path_top]]
        checkClustering(3, 5, by_top_level_dir)
        checkClustering(100, 200, by_top_level_dir)

if __name__ == '__main__':
    unittest.main()
