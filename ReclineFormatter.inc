<?php

/**
 * @file
 * Class ReclineFormatter.
 */

class ReclineFormatter {
  /**
   * Returns the type of the resource based on response headers.
   */
  public function getType($item) {
    if (isset($item['uri'])) {
      $url = $item['uri'];
    }
    else {
      if (isset($item['url'])) {
        $url = $item['url'];
      }
    }
    // Determine if we have the uri of a file or if it is a link to a file or a
    // link to an api.
    if (is_numeric(strpos($url, 'public://'))) {
      return 'file';
    }
    else {
      $response = drupal_http_request($url);
      dpm($response, 'formatter response');
      if ($response->code != '200') {
        return 'none';
      }
      else {
        $matches = array();
        if (preg_match('/text\/([a-z]*)/', $response->headers['content-type'], $matches) == 1) {
          return $matches[1];
        }
        else {
          if (preg_match('/application\/([a-z]*)/', $response->headers['content-type'], $matches) == 1) {
            return $matches[1];
          }
        }
      }
    }
    return 'none';
  }

  /**
   * Returns json data from the request.
   */
  public function getJsonData($item) {
    if (isset($item['uri'])) {
      $url = $item['uri'];
    }
    else {
      if (isset($item['url'])) {
        $url = $item['url'];
      }
    }
    $response = drupal_http_request($url);
    return $response->data;
  }

  /**
   * Returns url.
   */
  public function getUrl($item) {
    if (isset($item['uri'])) {
      return $item['uri'];
    }
    else {
      if (isset($item['url'])) {
        return $item['url'];
      }
    }
  }

  /**
   * Returns a url to a map from geojson.io.
   */
  public function getGeojsonData($url) {
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
}
