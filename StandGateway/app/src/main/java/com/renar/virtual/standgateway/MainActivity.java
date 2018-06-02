package com.renar.virtual.standgateway;

import android.app.IntentService;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.support.v4.content.LocalBroadcastManager;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.ScrollView;
import android.widget.TextView;

import com.google.common.collect.FluentIterable;

import java.io.IOException;
import java.util.Objects;
import java.util.UUID;

public class MainActivity extends AppCompatActivity {
  @Override
  protected void onCreate(final Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);
    LocalBroadcastManager.getInstance(this).registerReceiver(
        new BroadcastReceiver() {
          @Override
          public void onReceive(final Context context, final Intent intent) {
            log(intent.getStringExtra("message"));
          }
        },
        new IntentFilter(Constants.LOG_ACTION));
  }

  public void connectBluetooth(final View view) {
    log("Starting bluetooth");
    final BluetoothAdapter bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
    if (bluetoothAdapter == null) {
      log("adapter is null, bluetooth not supported by device");
      return;
    }

    if (!bluetoothAdapter.isEnabled()) {
      log("bluetooth not enabled, requesting permission");
      startActivityForResult(new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE),
          Constants.REQUEST_ENABLE_BT);
    } else {
      runBluetooth(bluetoothAdapter);
    }
  }

  @Override
  protected void onActivityResult(final int requestCode, final int resultCode, final Intent data) {
    switch (requestCode) {
      case Constants.REQUEST_ENABLE_BT:
        if (resultCode == RESULT_OK) {
          runBluetooth();
        } else {
          log("bluetooth enable not successful, resultCode " + resultCode);
        }
        break;
    }
  }

  private void runBluetooth() {
    runBluetooth(BluetoothAdapter.getDefaultAdapter());
  }

  private void runBluetooth(final BluetoothAdapter bluetoothAdapter) {
    // assumes bluetooth existence already asserted
    Objects.requireNonNull(bluetoothAdapter);
    log("bluetooth ready, searching connected devices");
    final BluetoothDevice standControllerDevice = FluentIterable
        .from(bluetoothAdapter.getBondedDevices())
        .firstMatch(device -> device.getName().equals(Constants.STAND_CONTROLLER_NAME))
        .orNull();
    if (standControllerDevice == null) {
      log(Constants.STAND_CONTROLLER_NAME + " not found among paired devices. giving up");
      return;
    }

    log("found device " + Constants.STAND_CONTROLLER_NAME);

    startService(new Intent(this, StandControllerCommService.class)
        .setAction(Constants.START_COMMS_ACTION)
        .putExtra("device.address", standControllerDevice.getAddress()));
  }

  private void log(final Object o) {
    log(Objects.toString(o));
  }

  private void log(final String message) {
    runOnUiThread(() -> {
      this.<TextView>findViewById(R.id.messageLog).append(message + "\n");
      final ScrollView messageLogScrollView = findViewById(R.id.messageLogView);
      messageLogScrollView.post(() -> messageLogScrollView.fullScroll(View.FOCUS_DOWN));
    });
  }

  public static class StandControllerCommService extends IntentService {

    public StandControllerCommService() {
      super(StandControllerCommService.class.getSimpleName());
    }

    @Override
    protected void onHandleIntent(final Intent intent) {
      switch (Objects.requireNonNull(intent.getAction())) {
        case Constants.START_COMMS_ACTION:
          connectToDevice(intent.getStringExtra("device.address"));
          break;
      }
    }

    private void connectToDevice(final String deviceAddress) {
      log("connecting to device at " + deviceAddress);
      // magic uuid for bluetooth module jy-mcu hc-06 *shrug*
      final UUID uuid = UUID.fromString("00001101-0000-1000-8000-00805f9b34fb");
      final BluetoothAdapter bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
      if (bluetoothAdapter == null) {
        log("stop turning off bluetooth");
        return;
      }

      final BluetoothDevice device = bluetoothAdapter.getRemoteDevice(deviceAddress);

      try {
        final BluetoothSocket socket = device.createRfcommSocketToServiceRecord(uuid);
        socket.connect();
        log("successfully connected");
      } catch (final IOException e) {
        log("failed to open socket:" + e.getMessage());
        return;
      }

    }

    private void log(final String msg) {
      LocalBroadcastManager.getInstance(this)
          .sendBroadcast(new Intent(Constants.LOG_ACTION)
              .putExtra("message", msg));
    }
  }

  private static final class Constants {
    static final String START_COMMS_ACTION = "com.renar.virtual.standgateway.START_COMMS";
    static final String LOG_ACTION = "com.renar.virtual.standgateway.LOG";
    static final String STAND_CONTROLLER_NAME = "StandController";
    static final int REQUEST_ENABLE_BT = 1;

    private Constants() {
    }

  }
}
