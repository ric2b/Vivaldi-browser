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

plugins {
  `java-library`
}

repositories {
  mavenCentral()
  google()
}

dependencies {
  implementation("androidx.annotation:annotation:1.6.0")

  // JUnit Test Support
  testImplementation("org.junit.jupiter:junit-jupiter-api:5.8.1")
  testImplementation("com.google.truth:truth:1.1.4")
  testImplementation("org.mockito:mockito-core:5.+")
  testImplementation("org.mockito:mockito-junit-jupiter:5.+")
  testRuntimeOnly("org.junit.jupiter:junit-jupiter-engine:5.8.1")
}

// Flattened directory layout
sourceSets {
  main {
    java {
      setSrcDirs(listOf("java"))
    }
  }
  test {
    java {
      setSrcDirs(listOf("test"))
    }
  }
}

tasks.test {
  useJUnitPlatform()
  jvmArgs = mutableListOf(
      // libnp_java_ffi.so
      "-Djava.library.path=$projectDir/../../target/debug",
      // ByteBuddy agent for mocks
      "-XX:+EnableDynamicAgentLoading"
  )
}
