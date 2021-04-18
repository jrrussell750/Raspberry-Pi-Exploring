<?php

session_start();

$server_ip = '10.100.0.1'; 
$server_port = 6001;  
$InputSelector = $_POST["InputSelector"]; 
$deviceid = $_SESSION["deviceID"];
$index = $deviceid - 1;
$device_code = array("\x81", "\x82", "\x83", "x\84", "\x85", "\x86");

// select input id for navigation control

if ($InputSelector == 'Prev')  // Prev button
{
  $inputid = "\x2b";
}
elseif ($InputSelector == 'Next')  // Next button
{
  $inputid = "\x2c"; 
}

// Compose 11 byte message for Niles GXR2

$message  = "\x00\x12\x00" . $device_code[$index] . "\x00\x0b\x61\x06" . $inputid . "\x00\xff";

//  Open a UDP socket to send to IP address 10.100.0.1, port # 6001

$socket = socket_create(AF_INET, SOCK_DGRAM, SOL_UDP);
socket_bind($socket, $_SESSION["LocalIP"], 6001);
socket_sendto($socket, $message, strlen($message), 0, $server_ip, $server_port);

// Return to the controller.php page

echo "<meta http-equiv = \"refresh\" content = \"0; URL=http://" . $_SESSION["LocalIP"] . "/controller.php\">";


?>