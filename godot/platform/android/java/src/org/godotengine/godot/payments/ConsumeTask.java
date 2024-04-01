/*************************************************************************/
/*  ConsumeTask.java                                                     */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
package org.godotengine.godot.payments;

import com.android.vending.billing.IInAppBillingService;

import android.content.Context;
import android.os.AsyncTask;
import android.os.RemoteException;
import android.util.Log;

abstract public class ConsumeTask {

	private Context context;

	private IInAppBillingService mService;
	public ConsumeTask(IInAppBillingService mService, Context context) {
		this.context = context;
		this.mService = mService;
	}

	public void consume(final String sku) {
		PaymentsCache pc = new PaymentsCache(context);
		Boolean isBlocked = pc.getConsumableFlag("block", sku);
		String _token = pc.getConsumableValue("token", sku);
		if (!isBlocked && _token == null) {
			// Consuming product, nothing to do
		} else if (!isBlocked) {
			return;
		} else if (_token == null) {
			// Token is not available
			this.error("No token for sku:" + sku);
			return;
		}
		final String token = _token;
		new AsyncTask<String, String, String>() {
			@Override
			protected String doInBackground(String... params) {
				try {
					// Requesting to release item
					int response = mService.consumePurchase(3, context.getPackageName(), token);
					if (response == 0 || response == 8) {
						return null;
					}
				} catch (RemoteException e) {
					return e.getMessage();
				}
				return "Some error";
			}

			protected void onPostExecute(String param) {
				if (param == null) {
					success(new PaymentsCache(context).getConsumableValue("ticket", sku));
				} else {
					error(param);
				}
			}
		}
				.execute();
	}

	abstract protected void success(String ticket);
	abstract protected void error(String message);
}
