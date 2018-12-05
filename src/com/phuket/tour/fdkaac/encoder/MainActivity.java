package com.phuket.tour.fdkaac.encoder;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;

public class MainActivity extends Activity {

	static {
		System.loadLibrary("audioencoder");
	}
	private Button encode_btn;

	private String pcmFilePath = "/mnt/sdcard/1.pcm";
	private String aacFilePath = "/mnt/sdcard/vocal_fdk.aac";

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);
		encode_btn = (Button) findViewById(R.id.encode_btn);
		encode_btn.setOnClickListener(new OnClickListener() {
			@Override
			public void onClick(View v) {
				long startTimeMills = System.currentTimeMillis();
				AudioEncoder audioEncoder = new AudioEncoder();
				audioEncoder.encode(pcmFilePath, 1, 128 * 1024, 48000, aacFilePath);
				int wasteTimeMills = (int)(System.currentTimeMillis() - startTimeMills);
				Log.i("success", "wasteTimeMills is : " + wasteTimeMills);
			}
		});
	}

}
