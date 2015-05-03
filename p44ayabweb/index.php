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
$queue = array();

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
      
      #machine {
        margin: 20px;
      }
      
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

      #machinerestart { text-align: center; background-color: #ffb300; width: 100%; }
      #machineready { text-align: center; background-color: #e0ff97; width: 100%; }

    </style>



    <script language="javascript1.2" type="text/javascript">

      $(function()
      {
        // document ready
        updateQueue();
      });

      var cursorUpdaterTimeout = 0;

      var patternWidth = 0;

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
          if (status<3) {
            $('#machinerestart').show();
            $('#machineready').hide();
          }
          else {
            $('#machineready').show();            
            $('#machinerestart').hide();
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
      
      
      function userCursorApply()
      {
        clearTimeout(cursorUpdaterTimeout);
        var query = {
          setPosition : $('#usercursor').width()
        };
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


      function applyNewWidth()
      {
        var width = $('#patternWidth').val();
        var query = {
          "setWidth" : width
        };
        $.ajax({
          url: '/api.php/machine',
          type: 'post',
          dataType: 'json',
          data: JSON.stringify(query),
          timeout: 3000
        }).done(function(response) {
          updateQueue();
        });
      }



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
    $qresp = false;
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
          $qresp = ayabJsonCall('/queue', array(
            'addFile' => $imagequeuedir . '/' . $queuefilename,
            'webURL' => $imagequeueurl . '/' . $queuefilename            
          ), true);
        }
      }
    }
        
    ?>
    <div id="machine">
      <div id="machineready" style="display:none;">Stricken läuft, Wagen hin-und-herschieben
        <button onClick="javascript:restartKnitting();">Neustart</button>        
      </div>
      <div id="machinerestart" style="display:none;">Stricken neustarten: Wagen ganz nach links, dann rechts bis 2*beep</div>
      <div id="queue">
        <div id="queueimages"></div>
        <div id="cursor"></div>
        <div id="usercursor" style="display:none;"></div>
      </div>
      <div id="userCursorControls" style="display:none;">
        <button id="userCursorApplyButton" onclick="javascript:userCursorApply();">Position setzen</button>
        <button id="userCursorCancelButton" onclick="javascript:userCursorCancel();">Abbrechen</button>
      </div>
      <div id="queuecontrol">
        <div id="errormessage"><?php echo($errormessage); ?></div>
        <form enctype="multipart/form-data" action="<?php echo $_SERVER['SCRIPT_NAME']; ?>" method="POST">
          <label for="newimage">Neues Bild hinzufügen:</label>
          <input name="newimage" id="newimage" type="file" maxlength="50000" accept="image/png" onchange="javascript:this.form.submit();"/>
        </form>
          <label for="patternWidth">Musterbreite:</label>
        <input type="number" name="patternWidth" id="patternWidth"/>&nbsp;<button onClick="javascript:applyNewWidth()">Breite neu setzen</button>
      </div>
    </div>

  </body>

</html>
