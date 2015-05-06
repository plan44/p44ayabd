<?php 

echo('<ul>');
$imagick = new Imagick();
$fonts = $imagick->queryFonts();
foreach($fonts as $font)
{
    echo('<li>' . $font . '</li>');
}
echo('</ul>');

/*
phpinfo();

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
*/

  
?>