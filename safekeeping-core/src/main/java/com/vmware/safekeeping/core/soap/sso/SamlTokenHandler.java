/*******************************************************************************
 * Copyright (C) 2019, VMware Inc
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
package com.vmware.safekeeping.core.soap.sso;

import java.util.logging.Logger;

import javax.xml.soap.SOAPException;
import javax.xml.ws.handler.soap.SOAPMessageContext;

import org.w3c.dom.DOMException;
import org.w3c.dom.Node;

import com.vmware.safekeeping.common.Utility;

public class SamlTokenHandler extends SSOHeaderHandler {

	private static final Logger logger = Logger.getLogger(SamlTokenHandler.class.getName());

	private final Node token;

	public SamlTokenHandler(final Node token) {
		if (!SsoUtils.isSamlToken(token)) {
			throw new IllegalArgumentException(Constants.ERR_NOT_A_SAML_TOKEN);
		}
		this.token = token;
	}

	@Override
	public boolean handleMessage(final SOAPMessageContext smc) {
		if (SsoUtils.isOutgoingMessage(smc)) {
			try {
				final Node securityNode = SsoUtils.getSecurityElement(SsoUtils.getSOAPHeader(smc));
				securityNode.appendChild(securityNode.getOwnerDocument().importNode(this.token, true));

			} catch (final DOMException e) {
				Utility.logWarning(logger, e);
				throw new RuntimeException(e);
			} catch (final SOAPException e) {
				Utility.logWarning(logger, e);
				throw new RuntimeException(e);
			}
		}

		return true;

	}
}
