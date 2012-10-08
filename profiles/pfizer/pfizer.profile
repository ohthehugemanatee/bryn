<?php

/**
 * Implements hook_disallowed_modules_alter().
 */
function pfizer_disallowed_modules_alter(&$modules) {
  $modules = array_diff($modules, array(
    'gardens_pdf_rendition',
    'gardens_site_variables',
    'node_export',
    'uuid',
    'janrain_client',
    'janrain_login',
  ));
}

/**
 * Implements hook_init().
 */
function pfizer_init() {
  // Disable the default janrain_login JS to use custom JS for Pfizer.
  $GLOBALS['conf']['janrain_login_add_default_js'] = FALSE;

  // Set the S3 bucket name for PDF rendition uploads (different on prod/staging)
  if (!empty($_ENV['AH_SITE_ENVIRONMENT']) && $_ENV['AH_SITE_ENVIRONMENT'] == 'production') {
    $GLOBALS['conf']['gardens_pdf_rendition_s3_bucket'] = 'pfizerbucket1';
  }
  else {
    $GLOBALS['conf']['gardens_pdf_rendition_s3_bucket'] = 'gsteamer.site-archives';
  }
}
