/// @file
/// @brief Embedded seed corpus shipped with the euxis binary.
///
/// Mirrors data/slopsquatting/seed-corpus.txt — keep the two in sync.
/// The raw string literal lets us avoid CMake-driven generated headers
/// while still keeping the corpus easy to diff.

#include <string_view>

namespace euxis::slopsquatting {

extern const std::string_view kEmbeddedSeedCorpus;
const std::string_view kEmbeddedSeedCorpus = R"SEED(
# euxis slopsquatting seed corpus (embedded copy of data/slopsquatting/seed-corpus.txt)

# ---- npm ----
npm: react-data-table = react-data-table-component
npm: simple-router = express
npm: easy-fetch = node-fetch
npm: quick-async = async
npm: lodash-utils = lodash
npm: lodash-helpers = lodash
npm: node-utils = lodash
npm: vue-router-dom = vue-router
npm: react-router-dom-v6 = react-router-dom
npm: axios-fetch = axios
npm: jest-runner-tests = jest
npm: typescript-strict = typescript
npm: ts-loader-v9 = ts-loader
npm: webpack-cli-v5 = webpack-cli
npm: babel-preset-typescript-strict = @babel/preset-typescript
npm: eslint-plugin-typescript-strict = @typescript-eslint/eslint-plugin
npm: nodejs-fs = fs
npm: nodejs-path = path
npm: discord-py-bot = discord.js
npm: openai-node-sdk = openai
npm: anthropic-node-sdk = @anthropic-ai/sdk
npm: google-cloud-storage-v3 = @google-cloud/storage
npm: aws-sdk-v3 = @aws-sdk/client-s3
npm: socket-io-client = socket.io-client
npm: rxjs-operators-v8 = rxjs

# ---- pypi ----
pypi: urlib3 = urllib3
pypi: python-dateutils = python-dateutil
pypi: pycript = cryptography
pypi: colourama = colorama
pypi: discord-py = discord.py
pypi: tensorflow-keras = tensorflow
pypi: keras-tensorflow = keras
pypi: django-rest = djangorestframework
pypi: flask-utils = flask
pypi: python-pdf = PyPDF2
pypi: sqlalchemy-orm = sqlalchemy
pypi: pandas-utils = pandas
pypi: numpy-utils = numpy
pypi: scikit-learn-utils = scikit-learn
pypi: openai-python-sdk = openai
pypi: anthropic-python-sdk = anthropic
pypi: aws-boto3 = boto3
pypi: google-cloud-sdk = google-cloud-storage
pypi: pytest-utils = pytest
pypi: requests-utils = requests
pypi: requestssss = requests
pypi: beautifulsoup = beautifulsoup4
pypi: scikit-image-utils = scikit-image
pypi: matplotlib-utils = matplotlib

# ---- cargo ----
cargo: tokio-utils = tokio
cargo: serde-utils = serde
cargo: serde-json-derive = serde
cargo: reqwest-client = reqwest
cargo: clap-derive-v4 = clap
cargo: async-std-utils = async-std
cargo: futures-utils = futures
cargo: anyhow-context = anyhow
cargo: tracing-subscriber-fmt = tracing-subscriber
cargo: rocket-framework = rocket
cargo: actix-web-utils = actix-web
cargo: axum-utils = axum
cargo: hyper-client = hyper
cargo: tonic-grpc = tonic
cargo: prost-protobuf = prost
cargo: chrono-utils = chrono
cargo: ring-crypto = ring
cargo: sha2-utils = sha2
cargo: rand-rng = rand

# ---- golang ----
golang: github.com/sirupsen/logrus-v2 = github.com/sirupsen/logrus
golang: github.com/gorilla/mux-v2 = github.com/gorilla/mux
golang: github.com/spf13/cobra-utils = github.com/spf13/cobra
golang: github.com/spf13/viper-utils = github.com/spf13/viper
golang: github.com/pkg/errors-v2 = github.com/pkg/errors
golang: github.com/stretchr/testify-v2 = github.com/stretchr/testify
golang: github.com/google/uuid-v2 = github.com/google/uuid
golang: github.com/golang/protobuf-v2 = google.golang.org/protobuf
golang: golang.org/x/sync-errgroup = golang.org/x/sync
golang: github.com/grpc/grpc-go = google.golang.org/grpc

# ---- maven ----
maven: org.apache.commons.commons-lang = org.apache.commons:commons-lang3
maven: com.google.code.gson-utils = com.google.code.gson:gson
maven: org.springframework.spring-boot-starter = org.springframework.boot:spring-boot-starter
maven: org.springframework.boot.starter-web = org.springframework.boot:spring-boot-starter-web
maven: org.junit.junit-jupiter = org.junit.jupiter:junit-jupiter

# ---- gem ----
gem: rails-utils = rails
gem: activerecord-utils = activerecord
gem: rspec-utils = rspec
gem: sidekiq-pro-utils = sidekiq
gem: devise-utils = devise
)SEED";

} // namespace euxis::slopsquatting
