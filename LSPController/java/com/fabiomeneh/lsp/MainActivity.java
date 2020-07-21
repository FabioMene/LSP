package com.fabiomeneh.lsp;

import android.content.SharedPreferences;

import android.app.*;
import android.content.*;
import android.os.*;

import android.view.*;
import android.widget.*;

import android.text.*;

import android.util.JsonReader;
import android.util.Log;

import java.io.*;
import java.net.*;
import java.util.*;
import java.util.UUID;
import java.util.concurrent.*;



public class MainActivity extends Activity {
    protected final MainActivity root = this;
    
    protected SharedPreferences prefs = null;
    
    public static final String PREFS_CONFIG = "config";
    public static final String CFG_API_BASE_URL = "api_base_url";
    
    public static final int STATUS_REFRESH_MS = 250;

    private RequestAsyncTask reqs;

    private EditText et_api_url;
    private TextView tv_status;
    private Button   btn_on_off;
    private SeekBar  sb_brightness;
    
    private LSPState lsp_state = new LSPState();

    @Override protected void onCreate(Bundle savedInstanceState){
        super.onCreate(savedInstanceState);
        
        // prefs
        prefs = getSharedPreferences(PREFS_CONFIG, MODE_PRIVATE);
        
        // widget refs
        setContentView(R.layout.main);
        
        et_api_url = (EditText)findViewById(R.id.api_base_url);
        tv_status  = (TextView)findViewById(R.id.status);
        btn_on_off = (Button)findViewById(R.id.btn_on_off);
        sb_brightness = (SeekBar)findViewById(R.id.sb_brightness);
        
        // EditText config
        et_api_url.setText(getApiBaseUrl());
        
        et_api_url.addTextChangedListener(new TextWatcher(){
            public void beforeTextChanged(CharSequence u0, int u1, int u2, int u3) {}  // stub
            public void onTextChanged(CharSequence u0, int u1, int u2, int u3) {}  // stub
            
            public void afterTextChanged(Editable e){
                String url = e.toString();
                
                if(!url.startsWith("http")) url = "http://" + url;
                if(!url.endsWith("/")) url = url + "/";
                
                SharedPreferences.Editor spe = prefs.edit();
                spe.putString(CFG_API_BASE_URL, url);
                spe.apply();
                
            }
        });
        
        // on off config
        btn_on_off.setOnClickListener(new View.OnClickListener(){
            public void onClick(View v){
                if(lsp_state.is_on){
                    tryMakeRequest("off");
                } else {
                    tryMakeRequest("on");
                }
            }
        });
        
        // seekbar cfg
        sb_brightness.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){
            public void onProgressChanged(SeekBar sb, int value, boolean from_user){
                if(from_user){
                    tryMakeRequest("brightness?" + value);
                }
            }
            
            public void onStartTrackingTouch(SeekBar sb) {}
            public void onStopTrackingTouch(SeekBar sb) {}
        });
        
        reloadInterruptButtons();
    }
    
    @Override public boolean onCreateOptionsMenu(Menu m){
        MenuInflater minf = getMenuInflater();
        minf.inflate(R.menu.main, m);
        
        return true;
    }
    
    @Override public boolean onOptionsItemSelected(MenuItem mi){
        switch(mi.getItemId()){
            case R.id.m_add_interrupt:
                final View diag_view = View.inflate(this, R.layout.diag_add_intr, null);
                
                AlertDialog.Builder add_builder = new AlertDialog.Builder(this);
                
                add_builder
                    .setTitle("Add interrupt")
                    .setView(diag_view)
                    .setCancelable(true)
                    .setPositiveButton("Save", new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface diag, int _u0) {
                            int ivect, iarg;
                            
                            String s_ivect = ((EditText)diag_view.findViewById(R.id.d_ai_vector)).getText().toString();
                            try {
                                ivect = Integer.parseInt(s_ivect);
                            } catch(NumberFormatException e){
                                spamToast("'" + s_ivect + "' is not a number");
                                return;
                            }
                            if(ivect < 0 || ivect > 3){
                                spamToast("The vector must be in range 0 to 3");
                                return;
                            }
                            
                            String s_iarg = ((EditText)diag_view.findViewById(R.id.d_ai_arg)).getText().toString();
                            try {
                                iarg = Integer.parseInt(s_iarg);
                            } catch(NumberFormatException e){
                                spamToast("'" + s_iarg + "' is not a number");
                                return;
                            }
                            if(iarg < 0 || iarg > 65535){
                                spamToast("The argument must be in range 0 to 65535");
                                return;
                            }
                            
                            
                            Set<String> intr_list = prefs.getStringSet("interrupts", null);
                            if(intr_list == null){
                                intr_list = new HashSet<String>();
                            }
                            
                            String id;
                            
                            do {
                                id = UUID.randomUUID().toString();
                            } while(intr_list.contains(id));
                            
                            
                            SharedPreferences.Editor edit = prefs.edit();
                            intr_list.add(id);
                            edit.putStringSet("interrupts", intr_list);
                            edit.putString("intr_" + id + "_label", ((EditText)diag_view.findViewById(R.id.d_ai_label)).getText().toString());
                            edit.putInt("intr_" + id + "_vector", ivect);
                            edit.putInt("intr_" + id + "_arg", iarg);
                            edit.apply();
                            
                            reloadInterruptButtons();
                            diag.cancel();
                        }
                    })
                    .setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface diag, int _u0) {
                            diag.cancel();
                        }
                    })
                    .show();
                break;
            
            case R.id.m_remove_interrupt:
                final Spinner sp = new Spinner(this);
                
                Set<String> _intr_list = prefs.getStringSet("interrupts", null);
                if(_intr_list == null){
                    _intr_list = new HashSet<String>();
                }
                
                final Object[] intr_list = _intr_list.toArray();
                
                ArrayAdapter<CharSequence> adapt = new ArrayAdapter<CharSequence>(this, android.R.layout.simple_spinner_item);
                for(Object id_o : intr_list){
                    adapt.add(prefs.getString("intr_" + (String)id_o + "_label", "(null)"));
                }
                adapt.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
                sp.setAdapter(adapt);
                
                AlertDialog.Builder remove_builder = new AlertDialog.Builder(this);
                remove_builder
                    .setTitle("Remove interrupt")
                    .setView(sp)
                    .setCancelable(true)
                    .setPositiveButton("Remove", new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface diag, int _u0) {
                            String id = (String)intr_list[sp.getSelectedItemPosition()];
                            
                            Set<String> intrs = prefs.getStringSet("interrupts", null);
                            
                            SharedPreferences.Editor edit = prefs.edit();
                            intrs.remove(id);
                            edit.putStringSet("interrupts", intrs);
                            edit.remove("intr_" + id + "_label");
                            edit.remove("intr_" + id + "_vector");
                            edit.remove("intr_" + id + "_arg");
                            edit.apply();
                            
                            reloadInterruptButtons();
                            diag.cancel();
                        }
                    })
                    .setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface diag, int _u0) {
                            diag.cancel();
                        }
                    })
                    .show();
                break;
            
            case R.id.m_load_program:
                EditText et = new EditText(this);
                
                AlertDialog.Builder load_builder = new AlertDialog.Builder(this);
                load_builder
                    .setTitle("Load program")
                    .setView(et)
                    .setCancelable(true)
                    .setPositiveButton("Load", new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface diag, int _u0) {
                            tryMakeRequest("load-program?" + et.getText().toString());
                            
                            diag.cancel();
                        }
                    })
                    .setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
                        public void onClick(DialogInterface diag, int _u0) {
                            diag.cancel();
                        }
                    })
                    .show();
                break;
            
            default:
                return super.onOptionsItemSelected(mi);
        }
        
        return true;
    }
    
    @Override public void onResume(){
        // Async start
        reqs = new RequestAsyncTask();
        reqs.execute();
        
        super.onResume();
    }
    
    @Override public void onPause(){
        // Async stop
        reqs.cancel(true);
        
        super.onPause();
    }
    
    
    protected View.OnClickListener intr_click_listener = new View.OnClickListener(){
        public void onClick(View v){
            InterruptButton ib = (InterruptButton)v;
            
            tryMakeRequest("interrupt?vector=" + ib.intr_vector + "&arg=" + ib.intr_arg);
        }
    };
    
    
    public void reloadInterruptButtons(){
        LinearLayout ll = (LinearLayout)findViewById(R.id.ll_interrupts);
        ll.removeAllViews();
        
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        
        Set<String> ids = prefs.getStringSet("interrupts", null);
        if(ids == null) return;
        
        for(String id : ids){
            InterruptButton ib = new InterruptButton(this);
            
            ib.intr_id = id;
            ib.intr_vector = prefs.getInt("intr_" + id + "_vector", -1);
            ib.intr_arg    = prefs.getInt("intr_" + id + "_arg", -1);
            
            ib.setText(prefs.getString("intr_" + id + "_label", "(null)"));
            ib.setOnClickListener(intr_click_listener);
            
            ll.addView(ib, lp);
        }
    }
    
    
    public String getApiBaseUrl(){
        if(prefs == null) return "";
        
        return prefs.getString(CFG_API_BASE_URL, "");
    }
    
    
    public void spamToast(String text){
        Toast.makeText(this, text, Toast.LENGTH_SHORT).show();
    }
    
    
    public void tryMakeRequest(String api_endpoint){
        if(!reqs.makeRequest(api_endpoint)){
            spamToast("API URL is not valid");
        }
    }


    private class RequestAsyncTask extends AsyncTask<Void, String, Void> {
        private LinkedBlockingQueue<String> lbq;
        
        public RequestAsyncTask(){
            lbq = new LinkedBlockingQueue<String>(5);
        }
        
        protected Void doInBackground(Void... _u){
            Log.i("RequestAsyncTask", "start");
            
            while(true){
                if(isCancelled()) break;
                
                URL url;
                boolean verbose = true;
                
                try {
                    String api_url = lbq.poll(STATUS_REFRESH_MS, TimeUnit.MILLISECONDS);
                    if(api_url == null){
                        api_url = getApiBaseUrl() + "status";
                        verbose = false;
                    }
                
                    url = new URL(api_url);
                } catch(Exception e){
                    publishProgress("error", "Error initializing the request: " + e);
                    continue;
                }
                
                
                HttpURLConnection huc;
                try {
                    huc = (HttpURLConnection)url.openConnection();
                } catch(IOException e){
                    publishProgress("error", "Connection error: " + e);
                    continue;
                }
                
                huc.setUseCaches(false);
                huc.setReadTimeout(STATUS_REFRESH_MS);
                int rc;
                
                if(verbose) publishProgress("info", "Sending request...");
                try {
                    huc.connect();
                    rc = huc.getResponseCode();
                } catch(IOException e){
                    publishProgress("error", "Request failed: " + e);
                    continue;
                }
                
                lsp_state.update_from_json_response(huc);
                
                if(verbose) switch(rc){
                    case 200:
                        publishProgress("info", "");
                        break;
                    
                    default:
                        publishProgress("error", "Request failed, http " + rc);
                        break;
                }
                
                publishProgress("status_update");
            }
            
            Log.i("RequestAsyncTask", "exit");
            
            // javac
            return null;
        }
        
        protected void onProgressUpdate(String... status){
            if(status[0].equals("info")){
                tv_status.setText(status[1]);
            } else if(status[0].equals("error")){
                tv_status.setText(status[1]);
            } else if(status[0].equals("status_update")){
                if(lsp_state.is_on){
                    btn_on_off.setText("OFF");
                } else {
                    btn_on_off.setText("ON");
                }
                
                sb_brightness.setProgress(lsp_state.brightness);
            }
        }
        
        public boolean makeRequest(String api_endpoint){
            String base = getApiBaseUrl();
            
            if("".equals(base)) return false;
            
            try {
                lbq.offer(base + api_endpoint);
            } catch(Exception e){
                // ...
                return false;
            }
            
            return true;
        }
    }
    
    public class LSPState {
        protected boolean is_on;
        protected int brightness;
        
        public LSPState(){
            is_on = false;
            brightness = 0;
        }
        
        public void update_from_json_response(HttpURLConnection huc){
            try {
                JsonReader jr = new JsonReader(new InputStreamReader(huc.getInputStream()));
                
                jr.beginObject();
                while(jr.hasNext()){
                    String key = jr.nextName();
                    
                    if     (key.equals("is_on"))      is_on      = jr.nextBoolean();
                    else if(key.equals("brightness")) brightness = jr.nextInt();
                    else jr.skipValue();
                }
                jr.endObject();
            } catch(Exception e) {}
        }
    }
}
