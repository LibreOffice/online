/* global require chai */

require('cypress-failed-log');

const chaiAlmost = require('chai-almost');

chai.use(chaiAlmost());
