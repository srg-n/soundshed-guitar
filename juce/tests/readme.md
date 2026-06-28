Nam calibration reference tests
```
cmake --build juce\builds --config Debug --target NamCalibrationProbeGatewayTests
$env:GUITARFX_NAM_COMPARE_MODEL_LIMIT='0'; 
.\juce\builds\Debug\NamCalibrationProbeGatewayTests.exe *> C:\temp\gateway-probe-latest.txt;

```