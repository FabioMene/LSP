package com.fabiomeneh.lsp;

import android.widget.Button;
import android.content.Context;

public class InterruptButton extends Button {
    public String intr_id;
    public int    intr_vector;
    public int    intr_arg;
    
    public InterruptButton(Context ctx){
        super(ctx);
    }
}
