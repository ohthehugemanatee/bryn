<?php

/**
 * @file
 * An API Client for making calls to the Janrain Capture web service.
 */

class JanrainCaptureApi {

  protected $args;
  protected $captureAddr;

  /**
   * Retrives API access credentials from settings.
   */
  function __construct() {
    $janrain_capture_main = variable_get('janrain_capture_main', array());
    $janrain_capture_optional = variable_get('janrain_capture_optional', array());

    $this->args = array();
    $this->args['client_id'] = isset($janrain_capture_main['capture_client_id'])
        ? $janrain_capture_main['capture_client_id']
        : '';
    $this->args['client_secret'] = isset($janrain_capture_main['capture_client_secret'])
        ? $janrain_capture_main['capture_client_secret']
        : '';

    $this->captureAddr = !empty($janrain_capture_main['capture_address'])
      ? $janrain_capture_main['capture_address']
      : '';
  }

  /**
   * Performs the HTTP request.
   *
   * @param string $command
   *   The Capture command to perform
   * @param array $arg_array
   *   The data set to pass via POST
   * @param string $access_token
   *   The client access token to use when performing user-specific calls
   *
   * @return mixed
   *   The HTTP request result data
   */
  protected function call($command, $arg_array = NULL, $access_token = NULL) {

    $url = "https://" . $this->captureAddr . "/$command";

    $headers = array(
      'Content-Type' => 'application/x-www-form-urlencoded',
      'User-Agent' => 'Drupal',
    );
    if (isset($access_token)) {
      $headers['Authorization'] = "OAuth $access_token";
    }
    $options = array('headers' => $headers);

    if (isset($arg_array)) {
      $arg_array = array_merge($arg_array, $this->args);
      $options['method'] = 'POST';
      $options['data'] = http_build_query($arg_array, '', '&');
    }
    $result = drupal_http_request($url, $options);

    if (!isset($result->data) || $result->code != '200') {
      $this->reportError($result);
      return FALSE;
    }

    $json_data = json_decode($result->data, TRUE);

    // NULL decoded value indicates a parse error.
    if (!isset($json_data)) {
      $json_data['stat'] = 'error';
      $json_data['code'] = '0';
      $json_data['error'] = 'JSON parse error for: ' . $result->data;
    }

    if ($json_data['stat'] == 'error') {
      $error = new stdClass();
      $error->code = $json_data['code'];
      $error->error = $json_data['error'];
      $this->reportError($error);
      return FALSE;
    }

    return $json_data;
  }

  /**
   * Updates session variables with Capture user tokens
   *
   * @param string $json_data
   *   The data received from the HTTP request containing the tokens
   */
  protected function updateCaptureSession($json_data) {
    $_SESSION['janrain_capture_access_token'] = $json_data['access_token'];
    $_SESSION['janrain_capture_refresh_token'] = $this->refreshTokenProtect($json_data['refresh_token']);
    $_SESSION['janrain_capture_expires_in'] = REQUEST_TIME + $json_data['expires_in'];

    $password_recover = (isset($json_data['transaction_state']['capture']['password_recover'])
        && $json_data['transaction_state']['capture']['password_recover'] == TRUE) ? TRUE : FALSE;
    $_SESSION['janrain_capture_password_recover'] = $password_recover;
    if (isset($json_data['transaction_state']['capture']['action'])) {
      $_SESSION['janrain_capture_action'] = $json_data['transaction_state']['capture']['action'];
    }
  }

  /**
   * Helper function to conceal the refresh token and set a cookie.
   *
   * @param string $raw_refresh_token
   *   The plain-text refresh token received from the capture server.
   *
   * @return string
   *   A base64 encoded, encrypted token.
   */
  protected function refreshTokenProtect($raw_refresh_token) {
    $len = strlen($raw_refresh_token);
    // Use a random pad of matching length to protect the token value.
    $pad = drupal_random_bytes($len);
    $cookie_pad = base64_encode($pad);
    // XOR the refresh token and encode the binary value.
    $protected_refresh_token = base64_encode($raw_refresh_token ^ $pad);
    // Use the same liftime and other params as for a Drupal session cookie.
    $params = session_get_cookie_params();
    $expire = $params['lifetime'] ? REQUEST_TIME + $params['lifetime'] : 0;
    setcookie('janrain_capture_pad', $cookie_pad, $expire, $params['path'], $params['domain'], FALSE, $params['httponly']);
    return $protected_refresh_token;
  }

  /**
   * Helper function to decode a refresh token using a pad from a cookie.
   *
   * @param string $protcted_refresh_token
   *   The base64 encoded, encrypted token.
   *
   * @return mixed
   *   A plain text token string, or NULL on failure.
   */
  protected function refreshTokenReveal($protected_refresh_token) {
    $raw_refresh_token = NULL;
    $bin = base64_decode($protected_refresh_token);
    if ($bin && isset($_COOKIE['janrain_capture_pad'])) {
      $pad = base64_decode($_COOKIE['janrain_capture_pad']);
      if ($pad && strlen($bin) == strlen($pad)) {
        $raw_refresh_token = $bin ^ $pad;
      }
    }
    return $raw_refresh_token;
  }

  /**
   * Helper function for the Engage web API wrappers.
   *
   * @param stdClass $result
   *   Result containing error code and message
   */
  protected function reportError($result) {
    watchdog('janrain_capture', 'Capture web API seems to be inaccessible due to "%error".', array('%error' => $result->code . ' ' . $result->error), WATCHDOG_WARNING);
    drupal_set_message(t('Capture web API seems to be inaccessible because of error "%error".', array('%error' => $result->code . ' ' . $result->error)), 'error');
  }

  /**
   * Perform the exchange to generate a new Access Token
   *
   * @param string $auth_code
   *   The authorization token to use for the exchange
   * @param string $redirect_uri
   *   The redirect_uri string to match for the exchange
   */
  public function newAccessToken($auth_code, $redirect_uri) {
    $command = "oauth/token";
    $arg_array = array(
      'code' => $auth_code,
      'redirect_uri' => $redirect_uri,
      'grant_type' => 'authorization_code',
    );

    $json_data = $this->call($command, $arg_array);
    if ($json_data) {
      $this->updateCaptureSession($json_data);
      return TRUE;
    }

    return FALSE;
  }

  /**
   * Retrieves a new access_token/refresh_token set
   *
   * @return boolean
   *   Boolean success or failure
   */
  function refreshAccessToken() {
    if (empty($_SESSION['janrain_capture_refresh_token'])) {
      return FALSE;
    }

    $refresh_token = $this->refreshTokenReveal($_SESSION['janrain_capture_refresh_token']);
    $command = "oauth/token";
    $arg_array = array(
      'refresh_token' => $refresh_token,
      'grant_type' => 'refresh_token',
    );

    $json_data = $this->call($command, $arg_array);

    if ($json_data) {
      $this->updateCaptureSession($json_data);
      return TRUE;
    }

    return FALSE;
  }

  /**
   * Retrives the user entity from Capture
   *
   * @param boolean $can_refresh
   *   Allow this function to refresh the token set if needed
   *
   * @return mixed
   *   The entity retrieved or null
   */
  public function loadUserEntity($can_refresh = TRUE) {
    if (empty($_SESSION['janrain_capture_access_token'])) {
      return NULL;
    }
    $user_entity = NULL;

    $need_to_refresh = FALSE;

    // Check if we need to refresh the access token.
    if (REQUEST_TIME >= $_SESSION['janrain_capture_expires_in']) {
      $need_to_refresh = TRUE;
    }
    else {
      $user_entity = $this->call('entity', array(), $_SESSION['janrain_capture_access_token']);
      if (isset($user_entity['code']) && $user_entity['code'] == '414') {
        $need_to_refresh = TRUE;
      }
    }

    // If necessary, refresh the access token and try to fetch the entity again.
    if ($need_to_refresh && $can_refresh) {
      if ($this->refreshAccessToken()) {
        $user_entity = $this->loadUserEntity(FALSE);
      }
    }

    // Return NULL if there is an error code.
    return isset($user_entity['code']) ? NULL : $user_entity;
  }

}
