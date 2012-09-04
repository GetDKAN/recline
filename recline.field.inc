<?php

/**
 * @file
 * Implements Recline.js field, based on file module's file field.
 */

/**
 * Implements hook_field_info().
 */
function recline_field_info() {
  return array(
    'recline_field' => array(
      'label' => t('Recline.js vizualization'),
      'description' => t('Visualize data with Recline.js.'),
      'settings' => array(
        'uri_scheme' => variable_get('file_default_scheme', 'public'),
      ),
     'instance_settings' => array(
       'file_extensions' => 'csv, xml',
       'file_directory' => '',
       'max_filesize' => '',
       'title_field' => 0,
       'alt_field' => 0,
      ),
     'default_widget' => 'recline_widget',
     'default_formatter' => 'recline_default_formatter',
    ),
  );
}

/**
 * Implements hook_field_formatter_info().
 */
function recline_field_formatter_info() {
  $formatters = array(
    'recline_default_formatter' => array(
      'label' => t('Recline.js visualization'),
      'field types' => array('recline_field'),
      // TODO: add some settings.
      'settings' => array(),
    ),
  );
  return $formatters;
}

/**
 * Implements hook_field_widget_info().
 */
function recline_field_widget_info() {
  return array(
    'recline_widget' => array(
      'label' => t('Recline upload'),
      'field types' => array('recline_field'),
      'settings' => array(
        'progress_indicator' => 'throbber',
      ),
      'behaviors' => array(
        'multiple values' => FIELD_BEHAVIOR_CUSTOM,
        'default value' => FIELD_BEHAVIOR_NONE,
      ),
    ),
  );
}

/**
 * Implements hook_field_instance_settings_form().
 */
function recline_field_instance_settings_form($field, $instance) {
  // TODO: only allow certain views from Recline.js here.
  $settings = $instance['settings'];

  // Use the file field instance settings form as a basis.
  $form = file_field_instance_settings_form($field, $instance);

  return $form;
}

/**
 * Implements hook_field_widget_form().
 */
function recline_field_widget_form(&$form, &$form_state, $field, $instance, $langcode, $items, $delta, $element) {

  $fieldset_info = element_info('fieldset');
  $process = array_merge($fieldset_info['#process'], array('recline_field_widget_form_process'));

  // Add display_field setting to field because file_field_widget_form() assumes it is set.
  $field['settings']['display_field'] = 0;
  // Have a separate fieldset for non-meat toppings

  foreach (recline_view_options() as $view => $label) {
    $element[$view] = array(
      '#title' => $label,
      '#type' => 'checkbox',
      '#weight' => 0,
      '#default_value' => isset($items[$delta][$view]) ? $items[$delta][$view] : '',
    );
  }

  $element['title'] = array(
    '#type' => 'item',
    '#title' => t('Recline Views'),
    '#weight' => -1,
  );
  $element['description'] = array(
    '#type' => 'item',
    '#markup' => t('These are views (as in MVC, not Drupal Views module) made available from Recline.js.'),
    '#weight' => 2,
  );

  $elements = file_field_widget_form($form, $form_state, $field, $instance, $langcode, $items, $delta, $element);

  foreach (element_children($elements) as $delta) {
    // If not using custom extension validation, ensure this is an image.
    $supported_extensions = array('csv');
    $extensions = isset($elements[$delta]['#upload_validators']['file_validate_extensions'][0]) ? $elements[$delta]['#upload_validators']['file_validate_extensions'][0] : implode(' ', $supported_extensions);
    $extensions = array_intersect(explode(' ', $extensions), $supported_extensions);
    $elements[$delta]['#upload_validators']['file_validate_extensions'][0] = implode(' ', $extensions);

    // Add all extra functionality provided by the recline widget.
    $elements[$delta]['#process'][] = 'recline_field_widget_process';
    $elements[$delta]['#default_value'] = !empty($items[$delta]) ? $items[$delta] : array();
    // This is a work-around because I couldn't figure out how to make "Recline
    // Views" a fieldset without errors.
    $elements[$delta]['file_description'] = array(
      '#type' => 'item',
      '#markup' => $elements[$delta]['#description'],
      '#weight' => -2,
    );
    unset($elements[$delta]['#description']);
  }

  return $elements;
}

/**
 * Process callback to remove one level of parentage
 */
function recline_field_widget_form_process($form, &$form_state, $complete) {
  return $form;
}
/**
 * Implements hook_field_load().
 */
function recline_field_load($entity_type, $entities, $field, $instances, $langcode, &$items, $age) {
  file_field_load($entity_type, $entities, $field, $instances, $langcode, $items, $age);
}

/**
 * Implements hook_field_is_empty().
 */
function recline_field_is_empty($item, $field) {
  return file_field_is_empty($item, $field);
}

/**
 * Implements hook_field_presave().
 */

function recline_field_presave($entity_type, $entity, $field, $instance, $langcode, &$items) {
  file_field_presave($entity_type, $entity, $field, $instance, $langcode, $items);
}

/**
 * Implements hook_field_insert().
 */
function recline_field_insert($entity_type, $entity, $field, $instance, $langcode, &$items) {
  file_field_insert($entity_type, $entity, $field, $instance, $langcode, $items);
}

/**
 * Implements hook_field_update().
 */
function recline_field_update($entity_type, $entity, $field, $instance, $langcode, &$items) {
  file_field_update($entity_type, $entity, $field, $instance, $langcode, $items);
}

/**
 * Implements hook_field_delete().
 */
function recline_field_delete($entity_type, $entity, $field, $instance, $langcode, &$items) {
  file_field_delete($entity_type, $entity, $field, $instance, $langcode, $items);
}

/**
 * Process the course file type element before displaying the field.
 */
function recline_field_widget_process($element, &$form_state, $form) {

  $instance = field_widget_instance($element, $form_state);

  $settings = $instance['settings'];
  $widget_settings = $instance['widget']['settings'];

  // Getting the item value of element.
  $item = $element['#value'];

  // Getting the delta of element.
  $delta = $element['#delta'];

  // Getting the delta of element.
  $item['fid'] = $element['fid']['#value'];

  $element['#theme'] = 'recline_widget';
  $element['remove_button']['#submit'] = array('recline_file_managed_file_submit');

  return $element;
}

/**
 * Form submission handler for remove buttons of recline fiile elements.
 *
 * @see file_managed_file_process()
 */
function recline_file_managed_file_submit($form, &$form_state) {
  // Determine whether it was the upload or the remove button that was clicked,
  // and set $element to the managed_file element that contains that button.
  $parents = $form_state['triggering_element']['#array_parents'];
  $button_key = array_pop($parents);
  $element = drupal_array_get_nested_value($form, $parents);

  if ($button_key == 'remove_button') {
    if ($element['#file']) {
      file_delete($element['#file']);
    }
    $values_element = $element['#extended'] ? $element['fid'] : $element;
    form_set_value($values_element, NULL, $form_state);
    drupal_array_set_nested_value($form_state['input'], $values_element['#parents'], NULL);
  }

  $form_state['rebuild'] = TRUE;
}

/**
 * Implements hook_field_formatter_view().
 */
function recline_field_formatter_view($entity_type, $entity, $field, $instance, $langcode, $items, $display) {

  $element = array();

  foreach ($items as $delta => $item) {
    $element[$delta] = array(
      '#theme' => 'recline_default_formatter',
      '#item' => $item,
    );
  }

  return $element;
}

/**
 * Returns HTML for an recline field formatter.
 *
 * @param $variables
 *   An associative array containing:
 *   - item: Associative array of recline field
 *
 * @ingroup themeable
 */

function theme_recline_default_formatter($variables) {
  $file = $variables['item'];

  $url = file_create_url($file['uri']);

  $icon = '';

  // Set options as per anchor format described at
  // http://microformats.org/wiki/file-format-examples
  $options = array(
    'attributes' => array(
      'type' => $file['filemime'] . '; length=' . $file['filesize'],
    ),
  );

  // Use the description as the link text if available.
  if (empty($file['description'])) {
    $link_text = $file['filename'];
  }
  else {
    $link_text = $file->description;
    $options['attributes']['title'] = check_plain($file['filename']);
  }
  drupal_add_library('system', 'ui.sortable');
  $recline = libraries_load('recline');
  $module_path = drupal_get_path('module', 'recline');
  drupal_add_js($module_path . '/recline.app.js');
  drupal_add_css($module_path . '/recline.css');
  $file_path = file_create_url($variables['item']['uri']);
  $settings['recline'] = array(
    'file' => $file_path,
    'grid' => (int)$variables['item']['grid'],
    'graph' => (int)$variables['item']['graph'],
    'map' => (int)$variables['item']['map'],
    'timeline' => (int)$variables['item']['timeline'],
    'transform' => (int)$variables['item']['transform'],
  );
  drupal_add_js($settings, 'setting');

  $file = l('Download', $url, $options);
  return '<div class="download">' . $file . '</div><span class="data-explorer-here"></span>';
}

/**
 * Returns HTML for an recline field widget.
 *
 * @param $variables
 *   An associative array containing:
 *   - item: Associative array of recline field
 *
 * @ingroup themeable
 */

function theme_recline_widget($variables) {
  $element = $variables['element'];

  $output = '';
  $output .= '<div class="recline-widget form-managed-file clearfix">';
  $output .= drupal_render_children($element);
  $output .= '</div>';

  return $output;
}

/**
 * Returns array of possible view types supported by Recline.
 */
function recline_view_options() {
  return array(
    'grid' => t('Grid'),
    'graph' => t('Graph'),
    'map' => t('Map'),
    'timeline' => t('Timeline'),
    'transform' => t('Transform'),
  );
}

