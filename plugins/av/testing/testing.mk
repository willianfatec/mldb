# Testing for the MLDB video capture plugin

$(eval $(call mldb_unit_test,MLDB-1956-audio-capture.js,av))
$(eval $(call program,video_device_test,avcodec avdevice avutil))
