<?php
/**
 * @file
 *  Provides the views plugin information.
 */

/**
 * Implements hook_views_plugins
 */
function views_load_more_views_plugins() {
  return array(
    'module' => 'views_load_more',
    'pager' => array(
      'load_more' => array(
        'title' => t('Show more pager (appends next page of results within the same page)'),
        'help' => t('views_load_more'),
        'handler' => 'views_plugin_pager_load_more',
        'uses options' => TRUE,
        'parent' => 'full',
      ),
    ),
  );
}