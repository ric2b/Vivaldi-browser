#!/usr/bin/env vpython3
# Copyright (c) 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import unittest
import sys
import os
import io
import tempfile
from unittest.mock import patch

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import create_temp_file


class TestCreateTempFileFromStdin(unittest.TestCase):

    def setUp(self):
        """Set up test environment: capture stdout and stderr."""
        self.held_stdout = sys.stdout
        self.held_stderr = sys.stderr
        self.new_stdout = io.StringIO()
        self.new_stderr = io.StringIO()
        sys.stdout = self.new_stdout
        sys.stderr = self.new_stderr

    def tearDown(self):
        """Clean up test environment: restore stdout and stderr."""
        sys.stdout = self.held_stdout
        sys.stderr = self.held_stderr

    def _read_content_and_delete_file(self, file_path):
        """Helper to read content of a temporary file."""
        with open(file_path, 'r', encoding='utf-8') as f:
            ret = f.read()
        os.remove(file_path)
        return ret

    @patch('sys.stdin', new_callable=io.StringIO)
    def test_basic_text_input(self, mock_stdin):
        """Test with basic multi-line text input."""
        test_content = "Hello, world!\nThis is a test.\n"
        mock_stdin.write(test_content)
        mock_stdin.seek(0)  # Rewind the mock stdin to the beginning

        create_temp_file.run()

        # Get the output printed to stdout (the temp file path)
        output = self.new_stdout.getvalue().strip()
        self.assertTrue(os.path.exists(output))  # Check if file was created

        # Verify content
        file_content = self._read_content_and_delete_file(output)
        self.assertEqual(self.new_stderr.getvalue(), "")  # No errors on stderr
        self.assertEqual(file_content, test_content)

    @patch('sys.stdin', new_callable=io.StringIO)
    def test_empty_input(self, mock_stdin):
        """Test with empty input from stdin."""
        test_content = ""
        mock_stdin.write(test_content)
        mock_stdin.seek(0)

        create_temp_file.run()
        output = self.new_stdout.getvalue().strip()
        self.assertTrue(os.path.exists(output))  # File should still be created
        file_content = self._read_content_and_delete_file(output)
        self.assertEqual(file_content, test_content)
        self.assertFalse(os.path.exists(output))
        self.assertEqual(self.new_stderr.getvalue(), "")

    @patch('sys.stdin', new_callable=io.StringIO)
    def test_with_prefix(self, mock_stdin):
        """Test if the --prefix argument is respected."""
        test_content = "Prefix test content.\n"
        mock_stdin.write(test_content)
        mock_stdin.seek(0)

        # Simulate argparse.args
        mock_args = argparse.Namespace(prefix="myprefix_", suffix=None)
        with patch('argparse.ArgumentParser.parse_args',
                   return_value=mock_args):
            create_temp_file.run(prefix=mock_args.prefix)

        output = self.new_stdout.getvalue().strip()
        self.assertTrue(os.path.exists(output))
        self.assertTrue(os.path.basename(output).startswith("myprefix_"))
        file_content = self._read_content_and_delete_file(output)
        self.assertEqual(file_content, test_content)

    @patch('sys.stdin', new_callable=io.StringIO)
    def test_with_suffix(self, mock_stdin):
        """Test if the --suffix argument is respected."""
        test_content = "Suffix test content.\n"
        mock_stdin.write(test_content)
        mock_stdin.seek(0)

        # Simulate argparse.args
        mock_args = argparse.Namespace(prefix=None, suffix=".log")
        with patch('argparse.ArgumentParser.parse_args',
                   return_value=mock_args):
            create_temp_file.run(suffix=mock_args.suffix)

        output = self.new_stdout.getvalue().strip()
        self.assertTrue(os.path.exists(output))
        self.assertTrue(os.path.basename(output).endswith(".log"))
        file_content = self._read_content_and_delete_file(output)
        self.assertEqual(file_content, test_content)


if __name__ == '__main__':
    unittest.main()
