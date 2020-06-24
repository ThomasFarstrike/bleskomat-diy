/*
	Copyright (C) 2020 Samotari (Charles Hill, Carlos Garcia Ortiz)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Affero General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Affero General Public License for more details.

	You should have received a copy of the GNU Affero General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

const BigNumber = require('bignumber.js');
const https = require('https');

const getRates = function(options) {
	if (typeof options !== 'object') {
		throw new Error('Invalid argument ("options"): Object expected');
	}
	if (!options.from) {
		throw new Error('Missing option: "from"');
	}
	if (!options.to) {
		throw new Error('Missing option: "to"');
	}
	return new Promise((resolve, reject) => {
		const done = function(error, result) {
			if (error) return reject(error);
			resolve(result);
		};
		const { from, to } = options;
		const uri = `https://api.coinbase.com/v2/exchange-rates?currency=${from}`;
		const req = https.request(uri, function(res) {
			let body = '';
			res.on('data', function(data) {
				body += data.toString();
			});
			res.on('end', function() {
				let rate;
				try {
					let json = JSON.parse(body);
					if (json.errors) {
						return done(new Error(JSON.stringify(json.errors)));
					}
					if (!json.data || !json.data.rates) {
						return done(new Error('Unexpected JSON response structure'));
					}
					if (!json.data.rates[to]) {
						return done(new Error(`Unsupported currency: "${to}"`));
					}
					rate = json.data.rates[to];
				} catch (error) {
					return done(error);
				}
				// Success!
				done(null, rate);
			});
		});
		req.on('error', error => {
			done(error);
		});
		req.end();
	});
};

const toSatoshis = function(amount, rate) {
	return (new BigNumber(amount))
			.dividedBy(rate)
			.times(1e8)
			.integerValue(BigNumber.ROUND_DOWN)
			.toNumber();
};

const toMilliSatoshis = function(amount, rate) {
	return toSatoshis(amount, rate) * 1000;
};

module.exports = {
	getRates,
	toSatoshis,
	toMilliSatoshis,
};