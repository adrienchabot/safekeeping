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
package com.vmware.safekeeping.core.command.options;

import com.vmware.safekeeping.core.type.enums.ExtensionManagerOperation;

public class ExtensionManagerOptions {
    public ExtensionManagerOptions(ExtensionManagerOperation emo) {
        extensionOperation = emo;
    }

    public ExtensionManagerOptions() {
    }

    public void setHealthInfoUrl(String healthInfoUrl) {
        this.healthInfoUrl = healthInfoUrl;
    }

    public void setServerThumbprint(String serverThumbprint) {
        this.serverThumbprint = serverThumbprint;
    }

    public void setServerInfoUrl(String serverInfoUrl) {
        this.serverInfoUrl = serverInfoUrl;
    }

    public ExtensionManagerOperation getExtensionOperation() {
        return extensionOperation;
    }

    public String getHealthInfoUrl() {
        return healthInfoUrl;
    }

    public String getServerThumbprint() {
        return serverThumbprint;
    }

    public String getServerInfoUrl() {
        return serverInfoUrl;
    }

    private String healthInfoUrl;
    private String serverThumbprint;
    private String serverInfoUrl;
    private ExtensionManagerOperation extensionOperation;

    public void setExtensionOperation(ExtensionManagerOperation extensionOperation) {
        this.extensionOperation = extensionOperation;
    }

}