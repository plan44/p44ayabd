<?php

define("DBGLVL", 0);

// configuration
$imagequeueurl = '/imgs';
$p44ayabd_host = 'localhost';
$p44ayabd_port = 9999;

// derived
$imagequeuedir = $_SERVER['DOCUMENT_ROOT'] . $imagequeueurl;

// init state
$errormessage = '';
$queue = array();

// defaults
if (!isset($_REQUEST['fontname']))
  $_REQUEST['fontname'] = 'Helvetica-Bold';
if (!isset($_REQUEST['fontsize']))
  $_REQUEST['fontsize'] = 45;




/// @param $aJsonRequest associative array representing JSON request to send to p44ayabd
function ayabJsonCall($aUri, $aJsonRequest = false, $aAction = false)
{
  global $p44ayabd_host, $p44ayabd_port;

  // wrap in mg44-compatible JSON
  $wrappedcall = array(
    'method' => $aAction ? 'POST' : 'GET',
    'uri' => $aUri,
  );
  if ($aJsonRequest!==false) $wrappedcall['data'] = $aJsonRequest;
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
    $result = json_decode($answer, true);
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

    <script src="js/jquery-1.9.1.min.js"></script>

    <style type="text/css">

      body {
        background-image: linear-gradient(
          to right,
          rgb(255, 63, 146)  0%,
          rgb(211, 121, 1)   18.5%,
          rgb(46, 165, 0)    37.0%,
          rgb(0, 166, 72)    40.7%,
          rgb(15, 149, 231)  74.0%,
          rgb(255, 51, 180)  96.9%,
          rgb(255, 63, 146) 100%
        );
        background-position: initial initial;
        background-repeat: initial initial;
        font-family: sans-serif;
      }

      #title {
        padding: 0px;
        padding-bottom: 20px;
      }
      #title table {
        width: 100%;
      }
      #projectname { width: 35%; }
      #credits { width: 65%; }
      #credits ul {
        margin-left: 20px;
        list-style: square;
        list-style-position: inside;
        padding: 0;
      }
      #title td {
        vertical-align: top;
        padding: 0;
      }
      #imgwait {
        padding: 20px;
        background-color: #ffa300;
      }
      #machine {
        padding: 20px;
        background-color: #d5e465;
      }
      #platform {
        margin-top: 20px;
        margin-left: auto;
        padding: 0;
      }
      #platform button {
        float: right;
      }

      #machineerr { text-align: center; background-color: #ff0000; width: 100%; }
      #machinerestart { text-align: center; background-color: #ffb300; width: 100%; }
      #machineready { text-align: center; background-color: #b3ff00; width: 100%; }

      #queue {
        position: relative;
        border: 1px solid red;
        padding: 0;
        margin-top: 10px;
        margin-bottom: 10px;
      }
      #queueimages {
        padding: 0;
        margin: 0;
        background-color: white;
      }
      #queueimages table {
        border-collapse: collapse;
        border-spacing: 0;
      }
      #queueimages td {
        padding: 0;
        margin: 0;
        vertical-align: top;
      }
      .imageblock {
        display: block;
      }
      .deletebutton {
        background-color: red;
        text-align: center;
        font-family: sans-serif;
        color: white;
        border: 0px;
        border-right: 1px solid white;
      }
      .currentsegment {
        background-color: #e0ff97;
        text-align: center;
        font-family: sans-serif;
        color: black;
        border: 0px;
        border-right: 1px solid white;
      }

      #queueimages img {
        margin: 0;
        padding: 0;
        display: block;
      }

      #cursor {
        position: absolute;
        pointer-events: none;
        border-right: 1px solid red;
        border-bottom: 1px solid red;
        background-color: rgba(131, 131, 131, 0.55);
        margin: 0; padding: 0;
        min-height: 20px;
        width: 20px;
        top: 0;
        left: 0;
      }

      #usercursor {
        position: absolute;
        pointer-events: none;
        border-right: 1px solid red;
        border-bottom: 1px solid red;
        background-color: rgba(255, 128, 128, 0.55);
        margin: 0; padding: 0;
        min-height: 20px;
        width: 20px;
        top: 0;
        left: 0;
      }

      #errormessage { font-weight: bold; color: red; }

    </style>



    <script language="javascript1.2" type="text/javascript">

      $(function()
      {
        // document ready
        updateQueue();
        $('#imgwait').hide();
      });

      var cursorUpdaterTimeout = 0;

      var patternWidth = 0;
      var ribber = false;
      var colors = 0;

      function updateMachineStatus()
      {
        $.ajax({
          url: '/api.php/machine',
          type: 'get',
          dataType: 'json',
          timeout: 3000
        }).done(function(response) {
          var status = response.result.status;
          // update machine status
          if (status<2) {
            $('#machineerr').show();
            $('#machinerestart').hide();
            $('#machineready').hide();
          }
          else if (status<3) {
            $('#machineerr').hide();
            $('#machinerestart').show();
            $('#machineready').hide();
          }
          else {
            $('#machineerr').hide();
            $('#machinerestart').hide();
            $('#machineready').show();
          }
        });
      }


      function updateCursor()
      {
        clearTimeout(cursorUpdaterTimeout);
        $.ajax({
          url: '/api.php/cursor',
          type: 'get',
          dataType: 'json',
          timeout: 3000
        }).done(function(response) {
          var cursor = response.result;
          $('#cursor').width(cursor.position);
          $('#cursor').height(patternWidth);
          $('#usercursor').height(patternWidth);
          updateMachineStatus();
          // poll every 2 sec again
          cursorUpdaterTimeout = setTimeout(function() { updateCursor(); }, 1000);
        });
      }

      function queueClick(event)
      {
        var x = event.layerX; // layerX,Y = relative to the closest positioned element
        var y = event.layerY;
        $('#usercursor').show();
        $('#usercursor').width(x);
        $('#userCursorControls').show();
      }


      function userCursorApply(boundary)
      {
        clearTimeout(cursorUpdaterTimeout);
        var query = {
          setPosition : $('#usercursor').width()
        };
        if (boundary) {
          query['boundary'] = true;
        }
        $.ajax({
          url: '/api.php/cursor',
          type: 'post',
          dataType: 'json',
          data: JSON.stringify(query),
          timeout: 3000
        }).done(function(response) {
          userCursorCancel();
          updateCursor();
        });
      }

      function userCursorCancel()
      {
        $('#usercursor').hide();
        $('#userCursorControls').hide();
      }



      function updateQueue()
      {
        $.ajax({
          url: '/api.php/queue',
          type: 'get',
          dataType: 'json',
          timeout: 3000
        }).done(function(response) {
          var state = response.result;
          var queue = state.queue;
          patternWidth = state.patternWidth;
          colors = state.colors;
          ribber = state.ribber;
          var queueHTML = '<table><tr onclick="javascript:queueClick(event);" height="' + patternWidth.toString() + '">';
          for (var i in queue) {
            var qe = queue[i];
            queueHTML += '<td width="' + qe.patternLength.toString() + '"><img id="' + i.toString() + '" src="' + qe.weburl + '"/></td>';
//             queueHTML += '<td width="' + qe.patternLength.toString() + '" height="' + patternWidth + '" style="background-image:url(\'' + qe.weburl + '\');"/></td>';
//             queueHTML += '<td style="min-width: ' + qe.patternLength.toString() + '; height: ' + patternWidth + '; background-image:url(\'' + qe.weburl + '\');"/></td>';
/*
             queueHTML +=
               '<td width="' + qe.patternLength.toString() + '">' +
               '<div class:"imageblock" style="min-width: ' + qe.patternLength.toString() + '; min-height: ' + patternWidth + '; background-image:url(\'' + qe.weburl + '\');"/></div>' +
               '</td>';
*/
          }
          queueHTML += '<td width="100%"></td>'; // rest
          queueHTML += '</tr><tr>';
          for (var i in queue) {
            queueHTML += '<td class="' + (i==state.cursor.entry ? 'currentsegment' : 'deletebutton')+ '" onClick="javascript:removeImg(' + i.toString() + ', true);">X</td>';
            i++;
          }
          queueHTML += '</tr><table>';
          $('#queueimages').html(queueHTML);
          $('#patternWidth').val(patternWidth.toString());
          $('#colors').val(colors.toString());
          $('#ribber').prop('checked', ribber);
          // also update cursor
          updateCursor();
        });
      }

      function removeImg(index, withDelete)
      {
        var query = {
          "removeFile" : index
        };
        if (withDelete) {
          query['delete'] = true;
        }
        $.ajax({
          url: '/api.php/queue',
          type: 'post',
          dataType: 'json',
          data: JSON.stringify(query),
          timeout: 3000
        }).done(function(response) {
          updateQueue();
        });
      }

      function restartKnitting()
      {
        var query = {
          "restart" : true
        };
        $.ajax({
          url: '/api.php/machine',
          type: 'post',
          dataType: 'json',
          data: JSON.stringify(query),
          timeout: 3000
        }).done(function(response) {
          updateMachineStatus();
        });
      }


      function applyNewParams()
      {
        var width = $('#patternWidth').val();
        var ribber = $('#ribber').is(':checked');
        var colors = $('#colors').val();
        $.ajax({
          url: '/api.php/machine',
          type: 'post',
          dataType: 'json',
          data: JSON.stringify({ "setWidth" : width, "setRibber":ribber, "setColors":colors }),
          timeout: 3000
        }).done(function(response) {
          updateQueue();
        });
      }


      function restartPlatform(withHalt)
      {
        var query = {
        };
        if (withHalt)
          query['shutdown'] = true;
        else
          query['restart'] = true;
        $.ajax({
          url: '/api.php/platform',
          type: 'post',
          dataType: 'json',
          data: JSON.stringify(query),
          timeout: 3000
        });
      }



    </script>

  </head>

  <body>
    <div id="title">
      <table>
        <tr>
          <td id="projectname"><h1>p44 AYAB web<br/>@<a href="<?php echo $_SERVER['SCRIPT_NAME']; ?>"><?php echo $_SERVER['SERVER_ADDR']; ?></a></h1></td>
          <td id="credits">
            <p>Endlos-Text/Symbolband-Strickmaschine basierend auf:</p>
            <ul>
              <li>Hardware + Firmware von <a href="http://www.ayab-knitting.com" target="_blank">AYAB (All Yarns Are Beautiful)</a></li>
              <li>Strickmaschine KH930 und Knowhow von <a href="http://zurich.fablab.ch" target="_blank">Fablab Zürich</a></li>
              <li><a href="http://www.arduino.cc" target="_blank">Arduino</a></li>
              <li><a href="https://www.raspberrypi.org" target="_blank">Raspberry Pi</a></li>
              <li>p44ayabd, Web-Interface und setup von <a href="" target="_blank">luz/plan44.ch</a></li>
            </ul>
          </td>
        </tr>
      </table>
    </div>

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
          ayabJsonCall('/queue', array(
            'addFile' => $imagequeuedir . '/' . $queuefilename,
            'webURL' => $imagequeueurl . '/' . $queuefilename
          ), true);
        }
      }
    }
    else if (isset($_REQUEST['addnewtext'])) {
      // get text
      $text = $_REQUEST['newtext'];
      $fontname = $_REQUEST['fontname'];
      $fontsize = $_REQUEST['fontsize'];
      // get text attributes
      if (empty($fontname))
        $fontname = 'Helvetica-Bold';
      if ($fontsize<9 || $fontsize>300)
        $fontsize = 45;

      ob_end_flush();
      echo('<div id="imgwait"><p>Bitte warten, der Raspberry Pi ist nicht so schnell beim Berechnen von Bildern...</p><ul>');
      flush();

      // simulate time it takes on a raspberry
      echo('<li>warte sinnlos 2 Sekunden</li>'); flush();
      sleep(2);


      // get width (= text height)
      $s = ayabJsonCall('/queue', false, false);
      $state = $s['result'];
      $patternWidth = $state['patternWidth'];


      // Create Imagick objects
      echo('<li>initialisiere</li>'); flush();
      $image = new Imagick();
      $draw = new ImagickDraw();
      $color = new ImagickPixel('black');
      $background = new ImagickPixel('white');

      // Set Font properties
      //$draw->setFont('Arial');
      $draw->setFont($fontname);
      $draw->setFontSize($fontsize);
      $draw->setFillColor($color);
      $draw->setStrokeAntialias(true);
      $draw->setTextAntialias(true);

      echo('<li>bestimme Dimension des Textes</li>'); flush();
      // Get font metrics
      $metrics = $image->queryFontMetrics($draw, $text);

      // draw the text
      $topborder = 2;
      $spaceatend = 5;
      echo('<li>generiere Text</li>'); flush();
      $draw->annotation(0, $topborder+$metrics['ascender'], $text);

      // create image of correct size
      echo('<li>generiere Bild</li>'); flush();
      $image->newImage($spaceatend+$metrics['textWidth'], $topborder+$metrics['textHeight'], $background);
      $image->setImageFormat('png');
      echo('<li>zeichne Text ins Bild</li>'); flush();
      $image->drawImage($draw);

      // save to queue dir
      $queuefilename = strftime('%Y-%m-%d_%H.%M.%S') . '_generated_text';
      echo('<li>speichere Bild</li>'); flush();
      file_put_contents($imagequeuedir . '/' . $queuefilename, $image);

      // add to queue
      echo('<li>Füge Bild zum Strickmuster hinzu</li>');
      ayabJsonCall('/queue', array(
        'addFile' => $imagequeuedir . '/' . $queuefilename,
        'webURL' => $imagequeueurl . '/' . $queuefilename
      ), true);
      echo('<li>fertig!</li></ul></div>'); flush();
      ob_start();

    }

    ?>
    <div id="machine">
      <div id="machineready" style="display:none;">Stricken läuft, Wagen hin-und-herschieben
        <button onClick="javascript:restartKnitting();">Neustart</button>
      </div>
      <div id="machinerestart" style="display:none;">Stricken neustarten: Wagen ganz nach links, dann rechts bis 2*beep
        <button onClick="javascript:restartKnitting();">Neustart</button>
      </div>
      <div id="machineerr" style="display:none;">Nicht betriebsbereit, bitte warten</div>
      <div id="queue">
        <div id="queueimages"></div>
        <div id="cursor"></div>
        <div id="usercursor" style="display:none;"></div>
      </div>
      <div id="userCursorControls" style="display:none;">
        <button id="userCursorApplyButton" onclick="javascript:userCursorApply(false);">Position setzen</button>
        <button id="userCursorBoundaryButton" onclick="javascript:userCursorApply(true);">Position auf Bildanfang setzen</button>
        <button id="userCursorCancelButton" onclick="javascript:userCursorCancel();">Abbrechen</button>
      </div>
      <div id="queuecontrol">

        <div id="errormessage"><?php echo($errormessage); ?></div>

        <p>
          <form enctype="multipart/form-data" action="<?php echo $_SERVER['SCRIPT_NAME']; ?>" method="POST">
            <label for="newimage">Neues Bild hinzufügen:</label>
            <input name="newimage" id="newimage" type="file" maxlength="50000" accept="image/png" onchange="javascript:this.form.submit();"/>
          </form>
        </p>

        <p>
          <form action="<?php echo $_SERVER['SCRIPT_NAME']; ?>" method="POST">
            <label for="newtext">Neuen Text hinzufügen:</label>
            <input name="newtext" id="newtext" />&nbsp;<button name="addnewtext" type="submit">Text hinzufügen</button>
            <br/>
            <label for="fontname">Schriftart:</label>
            <select name="fontname" id="fontname">
              <?php
//               $imagick = new Imagick();
//               $fonts = $imagick->queryFonts();
//               foreach($fonts as $font) {
//                 echo('<option' . ($font==$_REQUEST['fontname'] ? ' selected="true"' : '') . ' value="' . $font . '">' . $font . '</option>');
//               }
              ?>
            </select>
            <label for="fontsize">Schriftgrösse:</label>
            <input type="number" min="9" max="300" name="fontsize" id="fontsize" value="<?php echo $_REQUEST['fontsize']; ?>" />
          </form>
        </p>

        <p>
          <form>
            <label for="patternWidth">Musterbreite:</label>
            <input type="number" min="10" max="200" name="patternWidth" id="patternWidth" />
            <label for="ribber">Doppelbett:</label>
            <input type="checkbox" name="ribber" id="ribber" value="1"/>
            <label for="colors">Anzahl Farben:</label>
            <input type="number" name="colors" min="2" max="4" id="colors" />
            &nbsp;<button onClick="javascript:applyNewParams()">Parameter neu setzen</button>
          </form>
        </p>

      </div>
    </div>

    <div id="platform">
      <button onClick="javascript:restartPlatform(false)">RaspberryPi neustarten</button>
      <button onClick="javascript:restartPlatform(true)">RaspberryPi herunterfahren</button>
    </div>

  </body>

</html>
