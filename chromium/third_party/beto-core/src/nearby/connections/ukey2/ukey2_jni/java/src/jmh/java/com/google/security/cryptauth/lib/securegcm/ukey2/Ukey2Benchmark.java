/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.security.cryptauth.lib.securegcm.ukey2;

import com.google.security.cryptauth.lib.securegcm.ukey2.D2DHandshakeContext.NextProtocol;
import com.google.security.cryptauth.lib.securegcm.ukey2.D2DHandshakeContext.Role;
import java.util.Random;
import java.util.concurrent.TimeUnit;
import org.openjdk.jmh.annotations.*;
import org.openjdk.jmh.infra.Blackhole;

/**
 * Benchmark for encoding and decoding UKEY2 messages over the JNI, analogous to `ukey2_benches.rs`.
 * The parameters and the operations also roughly matches the that of the Rust Criterion benchmark.
 * That said, since the benchmark infrastructure is different, there will inevitably be differences
 * the skews the number in certain ways – comparison of numbers from the different benchmarks should
 * compared on order-of-magnitudes only. To get the JNI overhead, for example, it would be better
 * use this JMH infra to measure a call into a no-op Rust function, which is a more apples-to-apples
 * comparison.
 *
 * <p>To run this benchmark, run cargo build -p ukey2_jni --release && ./gradlew jmh
 */
@State(Scope.Benchmark)
@OutputTimeUnit(TimeUnit.SECONDS)
@BenchmarkMode(Mode.Throughput)
public class Ukey2Benchmark {

  @State(Scope.Thread)
  public static class ConnectionState {
    D2DConnectionContextV1 connContext;
    D2DConnectionContextV1 serverConnContext;

    @Param({"10", "512", "1024"})
    int sizeKibs;

    @Param NextProtocol nextProtocol;

    byte[] inputBytes;

    @Setup
    public void setup() throws Exception {
      D2DHandshakeContext initiatorContext =
          new D2DHandshakeContext(Role.INITIATOR, new NextProtocol[] {nextProtocol});
      D2DHandshakeContext serverContext =
          new D2DHandshakeContext(Role.RESPONDER, new NextProtocol[] {nextProtocol});
      serverContext.parseHandshakeMessage(initiatorContext.getNextHandshakeMessage());
      initiatorContext.parseHandshakeMessage(serverContext.getNextHandshakeMessage());
      serverContext.parseHandshakeMessage(initiatorContext.getNextHandshakeMessage());
      connContext = initiatorContext.toConnectionContext();
      serverConnContext = serverContext.toConnectionContext();
      Random random = new Random();
      inputBytes = new byte[sizeKibs * 1024];
      random.nextBytes(inputBytes);
    }
  }

  @Benchmark
  @Fork(3)
  @Warmup(iterations = 2, time = 500, timeUnit = TimeUnit.MILLISECONDS)
  @Measurement(iterations = 5, time = 500, timeUnit = TimeUnit.MILLISECONDS)
  public void encodeAndDecode(ConnectionState state, Blackhole blackhole) throws Exception {
    byte[] encoded = state.connContext.encodeMessageToPeer(state.inputBytes, null);
    byte[] decoded = state.serverConnContext.decodeMessageFromPeer(encoded, null);
    blackhole.consume(decoded);
  }
}
