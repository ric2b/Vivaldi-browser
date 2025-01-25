<?php
/**
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 *
 *
 * Can be run with "php avifinfo_test.php" from this folder.
 */

require_once('../avifinfo.php');

function test_avifinfo_parser( $file_name, $expected_width, $expected_height,
                               $expected_bit_depth, $expected_num_channels ) {
  $features = array( 'width'        => false, 'height'       => false,
                     'bit_depth'    => false, 'num_channels' => false );
  $handle = fopen( $file_name, 'rb' );
  if ( $handle ) {
    $parser  = new Avifinfo\Parser( $handle );
    $success = $parser->parse_ftyp() && $parser->parse_file();
    fclose( $handle );
    if ( $success ) {
      $features = $parser->features->primary_item_features;
    }
  }

  if ( $features['width'] != $expected_width ||
       $features['height'] != $expected_height ||
       $features['bit_depth'] != $expected_bit_depth ||
       $features['num_channels'] != $expected_num_channels ) {
    echo 'Failure: '.$file_name.PHP_EOL;
  } else {
    echo 'Success: '.$file_name.PHP_EOL;
  }
}

test_avifinfo_parser('avifinfo_test_1x1.avif', 1, 1, 8, 3);
test_avifinfo_parser('avifinfo_test_2x2_alpha.avif', 2, 2, 8, 4);
test_avifinfo_parser('avifinfo_test_1x1_10b_nopixi_metasize64b_mdatsize0.avif',
                     1, 1, 10, 3);
test_avifinfo_parser('avifinfo_test_199x200_alpha_grid2x1.avif', 199, 200, 8, 4);
