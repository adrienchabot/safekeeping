/*******************************************************************************
 * Copyright (C) 2021, VMware Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/
package com.vmware.safekeeping.core.soap;

import java.net.MalformedURLException;
import java.net.URL;
import java.util.Map;

import com.vmware.safekeeping.core.exception.SafekeepingConnectionException;
import com.vmware.safekeeping.core.soap.helpers.LookupServiceHelper;
import com.vmware.vsphereautomation.lookup.RuntimeFaultFaultMsg;

class LookupService {
	private LookupServiceHelper lookupServiceHelper;

	private final URL lookupUrl;

	LookupService(final String lookupUrl) throws MalformedURLException {
		this.lookupUrl = new URL(String.format("https://%s/lookupservice/sdk", lookupUrl));
	}

	LookupServiceHelper connect() throws SafekeepingConnectionException {
		try {
			this.lookupServiceHelper = new LookupServiceHelper(getLookupServiceUrl());
			return this.lookupServiceHelper;
		} catch (final RuntimeFaultFaultMsg e) {
			throw new SafekeepingConnectionException(
					String.format("failed to connect to %s", getLookupServiceUrl().toString()), e);
		}
	}

	void disconnect() {
		this.lookupServiceHelper = null;
	}

	Map<String, String> findVimUrls() throws RuntimeFaultFaultMsg {
		return this.lookupServiceHelper.findVimUrls();
	}

	/**
	 * @return the lookupService
	 */
	public LookupServiceHelper getLookupServiceHelper() {
		return this.lookupServiceHelper;
	}

// TODO Remove unused code found by UCDetector
//     public LookupServiceHelper getlookupService() {
// 	return this.lookupService;
//     }

	public URL getLookupServiceUrl() {
		return this.lookupUrl;
	}
}
