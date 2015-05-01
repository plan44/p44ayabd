<?php

define("DBGLVL", 1);

// configuration
$imagequeueurl = '/imgs';
$p44ayabd_host = 'localhost';
$p44ayabd_port = 9999;

// derived
$imagequeuedir = $_SERVER['DOCUMENT_ROOT'] . $imagequeueurl;

// init state
$errormessage = '';
$queuefiles = array();


/// @param $aJsonRequest associative array representing JSON request to send to p44ayabd
function ayabJsonCall($aJsonRequest)
{
  global $p44ayabd_host, $p44ayabd_port;
  
  // wrap in mg44-compatible JSON
  $wrappedcall = array(
    'method' => 'POST',
    'uri' => '',
    'data' => $aJsonRequest
  );
  $request = json_encode($wrappedcall);  
  $fp = fsockopen($p44ayabd_host, $p44ayabd_port, $errno, $errstr, 10);
  if (!$fp) {
    $result = array('error' => 'cannot open TCP connection to ' . $p44ayabd_host . ':' . $p44ayabd_port);
  } else {
    fwrite($fp, $request);
    $answer = '';
    while (!feof($fp)) {
      $answer .= fgets($fp, 128);
    }
    fclose($fp);
    // convert JSON to associative php array
    $result = json_decode($answer);
  }
  return $result; 
}


  
?>

<!DOCTYPE html>
<html xml:lang="de">

  <head>
    <meta http-equiv="content-type" content="text/html; charset=utf-8">

    <meta name="viewport" content="width=device-width, initial-scale=1">

    <meta name="apple-mobile-web-app-capable" content="yes" />
    <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent" />

    <title id="title_model">p44 AYAB web</title>

    <style type="text/css">
      #queue { border: 1px solid red; margin: 20px; min-height: 20px;}
      
      #queuecontrol { margin: 20px; }
      
      #errormessage { font-weight: bold; color: red; }
      
    </style>

    <script language="javascript1.2" type="text/javascript">

    </script>

  </head>

  <body>
    <h1>p44 AYAB web</h1>
    
    <?php 

    if (DBGLVL>1) {
      echo('<h3>$_REQUEST</h3><ul>');
      foreach ($_REQUEST as $k => $v) {
        echo('<li>' . $k . ' = ' . $v . '</li>');
      }
      echo('</ul>');
    
      echo('<h3>$_SERVER</h3><ul>');
      foreach ($_SERVER as $k => $v) {
        echo('<li>' . $k . ' = ' . $v . '</li>');
      }
      echo('</ul>');

      printf ('<b>$_FILES:</b><br><pre>');
      print_r($_FILES);
      printf ("</pre>");
    }
      
    // check for new file upload
    if (isset($_FILES['newimage'])) {
      if (!isset($_FILES['newimage']['error'])) {
        $errormessage = sprintf('Problem uploading file');
      }
      else if ($_FILES['newimage']['error']!=0) {
        $errormessage = sprintf('Error %d uploading file', $_FILES['newimage']['error']);
      }
      else if ($_FILES['newimage']['type']!='image/png') {
        $errormessage = sprintf('Image file must be a PNG');
      }
      else {
        // seems ok, move to queue
        $queuefilename = strftime('%Y-%m-%d_%H.%M.%S') . '_' . $_FILES['newimage']['name'];
        if (!move_uploaded_file($_FILES['newimage']['tmp_name'], $imagequeuedir . '/' . $queuefilename)) {
          $errormessage = sprintf('Cannot move file to queue (queue dir not writable by web server?)');
        }
        else {
          // %%% hack
          array_push($queuefiles, $queuefilename);
          ayabJsonCall(array(
            'command' => 'addToQueue',
            'file' => $queuefilename            
          ));
        }
      }
    }
  
      
    ?>

    <div id="queue">
      <?php
      foreach ($queuefiles as $qi => $qf) {
        echo ('<img id="' . $qi . '" src="' . $imagequeueurl . '/' . $qf . '"/>');
      }
      ?>
    </div>
    <div id="queuecontrol">
      <div id="errormessage"><?php echo($errormessage); ?></div>
      <form enctype="multipart/form-data" action="<?php echo $_SERVER['SCRIPT_NAME']; ?>" method="POST">
        <label for="newimage">Set new image to knit</label>
        <input name="newimage" id="newimage" type="file" maxlength="50000" accept="image/png"/>
        <button type="submit">OK</button>
      </form>
    </div>


  </body>

</html>
