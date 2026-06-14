// Jenkins shared-library step for euxis. Vendor this file into your
// shared library at vars/euxisScan.groovy and call from a declarative
// Pipeline:
//
//   pipeline {
//     agent any
//     stages {
//       stage('Security audit') {
//         steps { euxisScan(mode: 'check', failOn: 'high') }
//       }
//     }
//   }

def call(Map args = [:]) {
    String mode    = args.mode    ?: 'check'
    String target  = args.target  ?: '.'
    String version = args.version ?: 'latest'
    String failOn  = args.failOn  ?: 'high'

    String image = "ghcr.io/sebastienrousseau/euxis:${version}"

    docker.image(image).inside('--user root --entrypoint=""') {
        sh """
            /euxis ${mode} ${target} \\
              --sarif euxis-results.sarif \\
              --sbom  euxis-sbom.cdx.json \\
              --junit euxis-junit.xml \\
              --evidence euxis-evidence.tar.gz \\
              --fail-on ${failOn}
        """
    }

    archiveArtifacts artifacts: 'euxis-evidence.tar.gz,euxis-results.sarif,euxis-sbom.cdx.json',
                     allowEmptyArchive: true,
                     fingerprint: true
    junit allowEmptyResults: true, testResults: 'euxis-junit.xml'
}
