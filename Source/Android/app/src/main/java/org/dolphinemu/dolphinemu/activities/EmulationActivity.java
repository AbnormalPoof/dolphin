package org.dolphinemu.dolphinemu.activities;

import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewTreeObserver;
import android.widget.FrameLayout;
import android.widget.ImageView;

import com.squareup.picasso.Callback;
import com.squareup.picasso.Picasso;

import org.dolphinemu.dolphinemu.NativeLibrary;
import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.fragments.EmulationFragment;

import java.util.List;

public final class EmulationActivity extends AppCompatActivity
{
	private View mDecorView;
	private ImageView mImageView;
	private FrameLayout mFrameEmulation;

	private boolean mDeviceHasTouchScreen;
	private boolean mSystemUiVisible;

	// So that MainActivity knows which view to invalidate before the return animation.
	private int mPosition;

	/**
	 * Handlers are a way to pass a message to an Activity telling it to do something
	 * on the UI thread. This Handler responds to any message, even blank ones, by
	 * hiding the system UI.
	 */
	private Handler mSystemUiHider = new Handler()
	{
		@Override
		public void handleMessage(Message msg)
		{
			hideSystemUI();
		}
	};
	private String mScreenPath;
	private FrameLayout mFrameContent;

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// Picasso will take a while to load these big-ass screenshots. So don't run
		// the animation until we say so.
		postponeEnterTransition();

		mDeviceHasTouchScreen = getPackageManager().hasSystemFeature("android.hardware.touchscreen");

		// Get a handle to the Window containing the UI.
		mDecorView = getWindow().getDecorView();

		// Set these options now so that the SurfaceView the game renders into is the right size.
		mDecorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
				View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
				View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);

		// Set the ActionBar to follow the navigation/status bar's visibility changes.
		mDecorView.setOnSystemUiVisibilityChangeListener(
				new View.OnSystemUiVisibilityChangeListener()
				{
					@Override
					public void onSystemUiVisibilityChange(int flags)
					{
						mSystemUiVisible = (flags & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION) == 0;

						if (mSystemUiVisible)
						{
							getSupportActionBar().show();
							hideSystemUiAfterDelay();
						}
						else
						{
							getSupportActionBar().hide();
						}
					}
				}
		);

		setContentView(R.layout.activity_emulation);

		mImageView = (ImageView) findViewById(R.id.image_screenshot);
		mFrameContent = (FrameLayout) findViewById(R.id.frame_content);
		mFrameEmulation = (FrameLayout) findViewById(R.id.frame_emulation_fragment);

		Intent gameToEmulate = getIntent();
		String path = gameToEmulate.getStringExtra("SelectedGame");
		String title = gameToEmulate.getStringExtra("SelectedTitle");
		mScreenPath = gameToEmulate.getStringExtra("ScreenPath");
		mPosition = gameToEmulate.getIntExtra("GridPosition", -1);

		Picasso.with(this)
				.load(mScreenPath)
				.noFade()
				.noPlaceholder()
				.into(mImageView, new Callback()
				{
					@Override
					public void onSuccess()
					{
						scheduleStartPostponedTransition(mImageView);
					}

					@Override
					public void onError()
					{
						// Still have to do this, or else the app will crash.
						scheduleStartPostponedTransition(mImageView);
					}
				});

		mImageView.animate()
				.withLayer()
				.setStartDelay(2000)
				.setDuration(500)
				.alpha(0.0f)
				.withStartAction(new Runnable()
				{
					@Override
					public void run()
					{
						mFrameEmulation.setVisibility(View.VISIBLE);
					}
				})
				.withEndAction(new Runnable()
				{
					@Override
					public void run()
					{
						mImageView.setVisibility(View.GONE);
					}
				});

		setTitle(title);

		// Instantiate an EmulationFragment.
		EmulationFragment emulationFragment = EmulationFragment.newInstance(path);

		// Add fragment to the activity - this triggers all its lifecycle callbacks.
		getFragmentManager().beginTransaction()
				.add(R.id.frame_emulation_fragment, emulationFragment, EmulationFragment.FRAGMENT_TAG)
				.commit();
	}

	@Override
	protected void onStart()
	{
		super.onStart();
		Log.d("DolphinEmu", "EmulationActivity starting.");
		NativeLibrary.setEmulationActivity(this);
	}

	@Override
	protected void onStop()
	{
		super.onStop();
		Log.d("DolphinEmu", "EmulationActivity stopping.");

		NativeLibrary.setEmulationActivity(null);
	}

	@Override
	protected void onPostCreate(Bundle savedInstanceState)
	{
		super.onPostCreate(savedInstanceState);

		// Give the user a few seconds to see what the controls look like, then hide them.
		hideSystemUiAfterDelay();
	}

	@Override
	public void onWindowFocusChanged(boolean hasFocus)
	{
		super.onWindowFocusChanged(hasFocus);

		if (hasFocus)
		{
			hideSystemUiAfterDelay();
		}
		else
		{
			// If the window loses focus (i.e. a dialog box, or a popup menu is on screen
			// stop hiding the UI.
			mSystemUiHider.removeMessages(0);
		}
	}

	@Override
	public void onBackPressed()
	{
		if (!mDeviceHasTouchScreen && !mSystemUiVisible)
		{
			showSystemUI();
		}
		else
		{
			// Let the system handle it; i.e. quit the activity TODO or show "are you sure?" dialog.
			EmulationFragment fragment = (EmulationFragment) getFragmentManager()
					.findFragmentByTag(EmulationFragment.FRAGMENT_TAG);
			fragment.notifyEmulationStopped();

			NativeLibrary.StopEmulation();
		}
	}

	public void exitWithAnimation()
	{
		runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				Picasso.with(EmulationActivity.this)
						.invalidate(mScreenPath);

				Picasso.with(EmulationActivity.this)
						.load(mScreenPath)
						.noFade()
						.noPlaceholder()
						.into(mImageView, new Callback()
						{
							@Override
							public void onSuccess()
							{
								showScreenshot();
							}

							@Override
							public void onError()
							{
								finish();
							}
						});
			}
		});
	}

	private void showScreenshot()
	{
		mImageView.setVisibility(View.VISIBLE);
		mImageView.animate()
				.withLayer()
				.setDuration(500)
				.alpha(1.0f)
				.withEndAction(afterShowingScreenshot);
	}

	private Runnable afterShowingScreenshot = new Runnable()
	{
		@Override
		public void run()
		{
			mFrameContent.removeView(mFrameEmulation);
			setResult(mPosition);
			finishAfterTransition();
		}
	};

	@Override
	public boolean onCreateOptionsMenu(Menu menu)
	{
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.menu_emulation, menu);
		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item)
	{
		switch (item.getItemId())
		{
			// Enable/Disable input overlay.
			case R.id.menu_emulation_input_overlay:
			{
				EmulationFragment emulationFragment = (EmulationFragment) getFragmentManager()
						.findFragmentByTag(EmulationFragment.FRAGMENT_TAG);

				emulationFragment.toggleInputOverlayVisibility();

				return true;
			}

			// Screenshot capturing
			case R.id.menu_emulation_screenshot:
				NativeLibrary.SaveScreenShot();
				return true;

			// Quicksave / Load
			case R.id.menu_quicksave:
				NativeLibrary.SaveState(9);
				return true;

			case R.id.menu_quickload:
				NativeLibrary.LoadState(9);
				return true;

			// Save state slots
			case R.id.menu_emulation_save_1:
				NativeLibrary.SaveState(0);
				return true;

			case R.id.menu_emulation_save_2:
				NativeLibrary.SaveState(1);
				return true;

			case R.id.menu_emulation_save_3:
				NativeLibrary.SaveState(2);
				return true;

			case R.id.menu_emulation_save_4:
				NativeLibrary.SaveState(3);
				return true;

			case R.id.menu_emulation_save_5:
				NativeLibrary.SaveState(4);
				return true;

			// Load state slots
			case R.id.menu_emulation_load_1:
				NativeLibrary.LoadState(0);
				return true;

			case R.id.menu_emulation_load_2:
				NativeLibrary.LoadState(1);
				return true;

			case R.id.menu_emulation_load_3:
				NativeLibrary.LoadState(2);
				return true;

			case R.id.menu_emulation_load_4:
				NativeLibrary.LoadState(3);
				return true;

			case R.id.menu_emulation_load_5:
				NativeLibrary.LoadState(4);
				return true;

			default:
				return super.onOptionsItemSelected(item);
		}
	}

	// Gets button presses
	@Override
	public boolean dispatchKeyEvent(KeyEvent event)
	{
		int action = 0;

		switch (event.getAction())
		{
			case KeyEvent.ACTION_DOWN:
				// Handling the case where the back button is pressed.
				if (event.getKeyCode() == KeyEvent.KEYCODE_BACK)
				{
					onBackPressed();
					return true;
				}

				// Normal key events.
				action = NativeLibrary.ButtonState.PRESSED;
				break;
			case KeyEvent.ACTION_UP:
				action = NativeLibrary.ButtonState.RELEASED;
				break;
			default:
				return false;
		}
		InputDevice input = event.getDevice();
		boolean handled = NativeLibrary.onGamePadEvent(input.getDescriptor(), event.getKeyCode(), action);
		return handled;
	}

	@Override
	public boolean dispatchGenericMotionEvent(MotionEvent event)
	{
		if (((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == 0))
		{
			return super.dispatchGenericMotionEvent(event);
		}

		// Don't attempt to do anything if we are disconnecting a device.
		if (event.getActionMasked() == MotionEvent.ACTION_CANCEL)
			return true;

		InputDevice input = event.getDevice();
		List<InputDevice.MotionRange> motions = input.getMotionRanges();

		for (InputDevice.MotionRange range : motions)
		{
			NativeLibrary.onGamePadMoveEvent(input.getDescriptor(), range.getAxis(), event.getAxisValue(range.getAxis()));
		}

		return true;
	}

	private void hideSystemUiAfterDelay()
	{
		// Clear any pending hide events.
		mSystemUiHider.removeMessages(0);

		// Add a new hide event, to occur 3 seconds from now.
		mSystemUiHider.sendEmptyMessageDelayed(0, 3000);
	}

	private void hideSystemUI()
	{
		mSystemUiVisible = false;

		mDecorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
				View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
				View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
				View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
				View.SYSTEM_UI_FLAG_FULLSCREEN |
				View.SYSTEM_UI_FLAG_IMMERSIVE);
	}

	private void showSystemUI()
	{
		mSystemUiVisible = true;

		mDecorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
				View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
				View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);

		hideSystemUiAfterDelay();
	}


	private void scheduleStartPostponedTransition(final View sharedElement)
	{
		sharedElement.getViewTreeObserver().addOnPreDrawListener(
				new ViewTreeObserver.OnPreDrawListener()
				{
					@Override
					public boolean onPreDraw()
					{
						sharedElement.getViewTreeObserver().removeOnPreDrawListener(this);
						startPostponedEnterTransition();
						return true;
					}
				});
	}
}
