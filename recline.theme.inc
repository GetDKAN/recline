<?php

/**
 * @file
 * Recline theme functions.
 */

/**
 * Returns HTML for link field formatter for recline field.
 *
 * @param array $variables
 *   An associative array containing:
 *   - item: Associative array of recline field
 *
 * @ingroup themeable
 */
function theme_recline_link_formatter($variables) {
  $url = theme_recline_url_formatter($variables);
  $output = l(
    $variables['item']['filename'],
    $url
  );
  return $output;
}

/**
 * Returns HTML for url field formatter for recline field.
 *
 * @param array $variables
 *   An associative array containing:
 *   - item: Associative array of recline field
 *
 * @ingroup themeable
 */
function theme_recline_url_formatter($variables) {
  $output = file_create_url($variables['item']['uri']);
  $output = url(
    $output,
    array('absolute' => TRUE)
  );
  return $output;
}

/**
 * Returns HTML for an recline field formatter.
 *
 * @param array $variables
 *   An associative array containing:
 *   - item: Associative array of recline field
 *
 * @ingroup themeable
 */
function theme_recline_default_formatter($vars) {
  if (isset($vars['item']['entity']->field_format['und'][0]['tid'])) {
    $format = taxonomy_term_load($vars['item']['entity']->field_format['und'][0]['tid']);
    $type = $format->name;
  }

  if (isset($vars['item']['url'])) {
    $url = $vars['item']['url'];
  }
  elseif (isset($vars['item']['uri'])) {
    $url = file_create_url($vars['item']['uri']);
  }

  $default_file = array(
    'filemime' => $type,
    'filesize' => '',
    'filename' => $url,
    'description' => '',
  );
  $file = isset($vars['item']['fid']) ? $vars['item'] : $default_file;
  if (is_object($file)) {
    $file = (array) $file;
  }
  $output = recline_build_icon($url, $type, $file['filename'], $file['filemime'], $file['filesize'], $file['description']);
  $request = drupal_http_request($url, array('timeout' => 10, 'method' => 'HEAD'));
  if ($request->code != 200) {
    $output['preview'] = recline_data_unavailable();
    return drupal_render($output);
  }

  switch ($type) {
    case 'json':
      $output['preview'] = recline_prettify_json($url);
      break;

    case 'csv':
      $output['preview'] = recline_default_formatter_output($url, $file, $vars);
      break;

    case 'geojson':
      $output['preview'] = recline_geojson_formatter_output_leaflet($url);
      break;

    case 'zip':
      if (substr($vars['item']['uri'], 0, 4) != 'http') {
        $output['preview'] = recline_format_item_archive($vars['item']['uri']);
      }
      elseif ($request->headers['content-length'] < 500000) {
        $ch = curl_init($url);
        $filename = file_directory_temp() . "/tmp-" . rand(100000, 999999) . ".zip";
        $fp = fopen($filename, "w");

        curl_setopt($ch, CURLOPT_FILE, $fp);
        curl_setopt($ch, CURLOPT_HEADER, 0);

        curl_exec($ch);
        curl_close($ch);
        fclose($fp);
        $output['preview'] = recline_format_item_archive($filename);
      }
      else {
        $output[] = recline_data_unavailable();
      }
      break;

    case 'html':
      $output['preview'] = recline_format_link_api($url);
      break;

    case 'image':
    case 'png':
    case 'gif':
    case 'jpg':
      $output['preview'] = recline_format_image($url);
      break;

    case 'arcgis':
    case 'rest':
      $output['preview'] = recline_preview_arcgis_feature($url);
      break;

    case 'rdf':
    case 'xml':
      $output['preview'] = recline_prettify_xml($url);
      break;

    case 'pdf':
      $output['preview'] = recline_format_link_api($url);
      break;

    case 'wms':
      // TODO

    default:
      $output[] = recline_data_unavailable();
  }

  return drupal_render($output);
}

/**
 * Formats image.
 */
function recline_format_image($url) {
  return array(
    '#type' => 'markup',
    '#markup' => '<img src="' . $url . '" alt="image preview"/>',
  );
}

/**
 * Builds output for a link of an api.
 */
function recline_format_link_api($url) {
  return array(
    'iframe' => array(
      '#type' => 'html_tag',
      '#tag' => 'iframe',
      '#attributes' => array(
        'src' => $url,
        'height' => 600,
        'width' => 900,
        'style' => '',
      ),
      '#value' => '',
    ),
  );
}

/**
 * Provide unavaiable status message.
 */
function recline_data_unavailable() {
  return array(
    '#type' => 'markup',
    '#markup' => t('Data preview unavailable.'),
    '#prefix' => '<div class="messages status">',
    '#suffix' => '</div>',
  );
}

/**
 * Prettify json.
 */
function recline_prettify_json($url) {

  $response = drupal_http_request($url, array('timeout' => 5));
  if ($response->code == '200') {
    $data = $response->data;
  }
  else {
    return recline_data_unavailable();
  }
  $decoded = drupal_json_decode($data);
  $types = array('FeatureCollection', 'GeometryCollection', 'Feature', 'Point', 'MultiPoint', 'LineString', 'MultiLineString', 'Polygon', 'MultiPolygon');

  foreach ($types as $type) {
    foreach ($decoded as $key => $value) {
      if ($type === $value) {
        return recline_geojson_formatter_output_leaflet($url);
      }
    }
  }

  libraries_load('jquery-jsonview');
  $module_path = drupal_get_path('module', 'recline');
  drupal_add_js(array('recline' => array('data' => $data)), 'setting');
  drupal_add_js($module_path . '/jsondataview.js');
  $json = array(
    'toggle_btn' => array(
      '#type' => 'markup',
      '#markup' => '<a id="toggle-btn" class="embed-link">Toggle view</a>',
    ),
    'data_json' => array(
      '#type' => 'markup',
      '#markup' => '<div id="recline-data-json"></div>',
    ),
  );
  return $json;
}

/**
 * Prettify xml.
 */
function recline_prettify_xml($url) {

  $response = drupal_http_request($url, array('timeout' => 5));
  if ($response->code == '200') {
    $data = recline_replace_lt_gt($response->data);
  }
  else {
    return recline_data_unavailable();
  }

  drupal_add_library('recline', 'highlight');
  drupal_add_js('jQuery(document).ready(function () {hljs.initHighlightingOnLoad();});', 'inline');
  $xml = array(
    'data_xml' => array(
      '#type' => 'markup',
      '#markup' => '<pre><code class="hljs xml">' . $data . '</code></pre>',
    ),
  );
  return $xml;
}

/**
 * Replace angle brackets.
 */
function recline_replace_lt_gt($data) {
  $replaced = preg_replace('/</', '&lt;', $data);
  return preg_replace('/>/', '&gt;', $replaced);
}

/**
 * Preview for arcgis and rest data.
 */
function recline_preview_arcgis_feature($url) {
  drupal_add_js('https://cdn.jsdelivr.net/leaflet/0.7.3/leaflet.js', 'external');
  drupal_add_js('https:////cdn.jsdelivr.net/leaflet.esri/1.0.0/esri-leaflet.js', 'external');
  drupal_add_css('https://cdn.jsdelivr.net/leaflet/0.7.3/leaflet.css', 'external');
  drupal_add_css('body { margin:0; padding:0; } #rest-map { position: relative; height: 500px; width: 100%;}', 'inline');

  $module_path = drupal_get_path('module', 'recline');
  drupal_add_js(array('recline' => array('url' => $url)), 'setting');
  drupal_add_js($module_path . '/restdataview.js');

  $output = array(
    'map' => array(
      '#type' => 'markup',
      '#markup' => '<div><div id="rest-map"></div></div>',
    ),
  );
  return $output;
}

/**
 * Gets url for geojson.io.
 */
function recline_get_geojson_data_URL($url) {
  $response = drupal_http_request($url);
  if ($response->code == '200') {
    $geojson_url = 'http://geojson.io/#data=data:application/json,' . $response->data;
    $geojson_response = drupal_http_request($geojson_url);
    if ($geojson_response->code == '200' && preg_match('/text\/html.*/', $geojson_response->headers['content-type']) == 1) {
      return $geojson_url;
    }
  }
  return '';
}

/**
 * Returns output for a geojson file.
 */
function recline_geojson_formatter_output($uri) {
  $url = file_create_url($uri);
  $item['url'] = recline_get_geojson_data_URL($url);
  if (!empty($item['url'])) {
    $item['title'] = $item['url'];
    return recline_format_link_api($item);
  }
  else {
    return '';
  }
}

/**
 * Returns output for a geojson file using leaflet library.
 */
function recline_geojson_formatter_output_leaflet($url) {
  $response = drupal_http_request($url);
  if ($response->code == '200') {
    $geojson = $response->data;
  }
  else {
    return ;
  }

  drupal_add_library('leaflet_widget', 'leaflet_core');
  $output = "var dataExtent = " . $geojson;
  $output .= "; var map = L.map('map');
    L.tileLayer('http://{s}.tile.osm.org/{z}/{x}/{y}.png', {
    attribution: 'Map data &copy; <a href=\"http://openstreetmap.org\">OpenStreetMap</a>'
    }).addTo(map);
    var geojson = L.geoJson(dataExtent).addTo(map);
    map.fitBounds(geojson.getBounds());";

  drupal_add_js($output, array('type' => 'inline', 'scope' => 'footer', 'weight' => 5));
  return array(
    'map' => array(
      '#type' => 'markup',
      '#markup' => '<div id="map" style="height:450px; width:100%"></div>',
    ),
  );
  return $output;
}

/**
 * Builds output render array for files if item is an archive.
 */
function recline_format_item_archive($uri) {
  // List files if item is an archive.
  try {
    $archiver = archiver_get_archiver($uri);
  }
  catch (Exception $e) {
    return recline_data_unavailable();
  }
  if ($archiver) {
    $module_path = drupal_get_path('module', 'recline');
    drupal_add_css($module_path . '/recline.css');
    $output['ziplist'] = array(
      '#theme' => 'item_list',
      '#prefix' => '<div id="recline-zip-list">',
      '#suffix' => '</div>',
      '#title' => t('%number files in this archive', array('%number' => count($archiver->listContents()))),
      '#items' => $archiver->listContents(),
    );
  }
  elseif ($format == 'zip') {
    $archivers_available = implode(', ', array_keys(archiver_get_info()));
    drupal_set_message(t('Unable to provide file listing for zip format on this system. Available archiver formats are: %s',
      array('%s' => $archivers_available)));
  }
  return $output;
}

/**
 * Builds icon for resource view.
 */
function recline_build_icon($url, $format, $filename, $filemime, $filesize, $description) {
  // Set options as per anchor format described at
  // http://microformats.org/wiki/file-format-examples
  $options = array(
    'attributes' => array(
      'type' => $filemime . '; length=' . $filesize,
      'data-format' => $format,
      'class' => array('format-label'),
      'title' => check_plain($filename),
    ),
  );

  $file = l($filename, $url, $options);

  return array(
    'download' => array(
      '#type' => 'markup',
      '#markup' => '<div class="download">' . $file . '</div>',
    ),
  );
}

/**
 * Builds output render array for field formatter.
 */
function recline_default_formatter_output($url, $file, $variables) {
  $dkan = $table = FALSE;
  // See if datastore is loaded, if so, prepare recline to view from it.
  if (module_exists('dkan_datastore_api') && module_exists('feeds_flatstore_processor') && function_exists('dkan_datastore_api_get_feeds_source')) {
    $source_id = dkan_datastore_api_get_feeds_source($variables['item']['entity']->nid);
    if ($table = feeds_flatstore_processor_table_name($source_id, $variables['item']['entity']->nid)) {
      if (db_table_exists($table)) {
        $dkan = $table;
      }
    }
  }
  $recline = '';

  libraries_load('lodash');
  libraries_load('backbone');
  $recline = libraries_load('recline');
  libraries_load('csv');
  libraries_load('mustache');
  libraries_load('moment');
  libraries_load('slickgrid');
  libraries_load('leaflet');

  libraries_load('flot');
  libraries_load('deep_diff');
  libraries_load('recline_deeplink');
  libraries_load('leaflet_markercluster');

  if (isset($variables['item']['grid']) &&
    isset($variables['item']['graph']) &&
    isset($variables['item']['map']) &&
    isset($variables['item']['embed'])) {
    if ($variables['item']['map'] || $variables['item']['grid'] || $variables['item']['graph'] || $variables['item']['embed']) {
      $uuid = isset($variables['item']['entity']->uuid) ? $variables['item']['entity']->uuid : FALSE;
      drupal_add_library('system', 'ui.sortable');

      $module_path = drupal_get_path('module', 'recline');
      drupal_add_js($module_path . '/backend.ckan_get.js');
      drupal_add_js($module_path . '/recline.app.js');
      drupal_add_css($module_path . '/recline.css');
      $settings['recline'] = array(
        'file' => $url,
        'delimiter' => $variables['item']['delimiter'],
        'grid' => (int) $variables['item']['grid'],
        'graph' => (int) $variables['item']['graph'],
        'map' => (int) $variables['item']['map'],
        'embed' => (int) $variables['item']['embed'],
        'uuid' => $uuid,
        'fileType' => $file['filemime'],
        'dkan' => $dkan,
        'ajax_timeout' => variable_get(RECLINE_AJAX_CALL_TIMEOUT, RECLINE_AJAX_CALL_DEFAULT_TIMEOUT),
      );
      drupal_add_js($settings, 'setting');
    }
  }
  else {
    $uuid = isset($variables['item']['entity']->uuid) ? $variables['item']['entity']->uuid : FALSE;
    drupal_add_library('system', 'ui.sortable');

    $module_path = drupal_get_path('module', 'recline');
    drupal_add_js($module_path . '/backend.ckan_get.js');
    drupal_add_js($module_path . '/recline.app.js');
    drupal_add_css($module_path . '/recline.css');
    $delimiter = isset($variables['item']['delimiter']) ? $variables['item']['delimiter'] : ",";
    $settings['recline'] = array(
      'file' => $url,
      'delimiter' => $delimiter,
      'uuid' => $uuid,
      'grid' => 1,
      'graph' => 1,
      'map' => 1,
      'embed' => 1,
      'fileType' => $file['filemime'],
      'dkan' => $dkan,
      'ajax_timeout' => variable_get(RECLINE_AJAX_CALL_TIMEOUT, RECLINE_AJAX_CALL_DEFAULT_TIMEOUT),
    );
    drupal_add_js($settings, 'setting');
  }
  if ($recline) {
    // Necessary to fix https://github.com/mapbox/mapbox.js/issues/339
    $leaflet_imgs = $recline['library path'] . '/vendor/leaflet/0.7.3/images';
    drupal_add_js('L.Icon.Default.imagePath = "/' . $leaflet_imgs . '"', 'inline');
  }

  $node = $variables['item']['entity'];
  $embed = theme('recline_embed_button', array('node' => $node));

  if (isset($settings['recline']['embed']) && $settings['recline']['embed']) {
    $output['embed'] = array(
      '#type' => 'markup',
      '#markup' => $embed,
    );
  }
  $output['data_explorer'] = array(
    '#type' => 'markup',
    '#markup' => '<span class="data-explorer"></span>',
  );
  return $output;
}

/**
 * Implements hook_preprocess_HOOK().
 *
 * Process variables for recline-embed-button.tpl.php.
 */
function template_preprocess_recline_embed_button(&$variables) {
  $node = $variables['node'];
  $variables['embed_url'] = url('node/' . $node->nid . '/recline-embed', array('absolute' => TRUE));
}

/**
 * Returns HTML for an recline field widget.
 *
 * @param array $variables
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
