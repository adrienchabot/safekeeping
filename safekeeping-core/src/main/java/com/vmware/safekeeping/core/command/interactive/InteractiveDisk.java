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
package com.vmware.safekeeping.core.command.interactive;

import java.util.List;

import com.vmware.safekeeping.core.command.results.support.OperationState;
import com.vmware.safekeeping.core.control.info.ExBlockInfo;
import com.vmware.safekeeping.core.control.info.TotalBlocksInfo;
import com.vmware.safekeeping.core.type.ManagedEntityInfo;

public interface InteractiveDisk {
    /**
     * @param dumpFileInfo
     */
    void dumpFailure(final ExBlockInfo blockInfo);

    /**
     * @param dumpFileInfo
     */
    void dumpSuccess(final ExBlockInfo blockInfo);

    void endDumpsTotalCalculation(final TotalBlocksInfo totalDumpInfo);

    void endDumpThreads(final OperationState state);

    /**
     * @return the totalDumpsInfo
     */
    List<TotalBlocksInfo> getTotalDumpsInfo();

    void startDumpsTotalCalculation();

    void startDumpThreads();

    ManagedEntityInfo getEntity();

}
