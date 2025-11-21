CREATE TABLE IF NOT EXISTS list_entries (
	entryno INTEGER PRIMARY KEY,
	billing_authority_code CHAR(4),
	ndr_community_code CHAR(6),
	ba_reference_number CHAR(25),
	primary_secondary_description_code VARCHAR(5),
	primary_description_text VARCHAR(60),
	uarn INTEGER UNIQUE,
	full_property_identifier VARCHAR(416),
	firm_name VARCHAR(50),
	number_or_name VARCHAR(72),
	street VARCHAR(36),
	town VARCHAR(36),
	postal_district VARCHAR(36),
	county VARCHAR(36),
	postcode VARCHAR(8),
	effective_date CHAR(11),
	composite_indicator CHAR(1),
	rateable_value NUMERIC,
	appeal_court_settlement_code VARCHAR(8),
	assessment_reference NUMERIC,
	list_alteration_date CHAR(11),
	scat_code_and_suffix CHAR(4),
	sub_street_level_three VARCHAR(36),
	sub_street_level_two VARCHAR(36),
	sub_street_level_one VARCHAR(36),
	case_number INTEGER,
	current_from_date CHAR(11),
	current_to_date CHAR(11)
);

CREATE TABLE IF NOT EXISTS list_entries_historic (
	ba_code VARCHAR(4),
	ndr_community_code VARCHAR(6),
	ba_reference_no VARCHAR(25),
	primary_secondary_description_code VARCHAR(5),
	primary_description_text VARCHAR(60),
	uarn INTEGER UNIQUE,
	effective_date CHAR(11),
	composite_indicator CHAR(1),
	rateable_value NUMERIC,
	appeal_settlement_code VARCHAR(8),
	assessement_reference NUMERIC,
	list_alteration_date CHAR(11),
	scat_code_and_suffix CHAR(4),
	case_number INTEGER,
	historic_from_date CHAR(11),
	historic_to_date CHAR(11)
);

CREATE TABLE IF NOT EXISTS related_list_entries (
	record_type CHAR(2),
	assessment_reference INTEGER PRIMARY KEY,
	uarn INTEGER,
	ba_code CHAR(4),
	firm_name VARCHAR(50),
	number_or_name VARCHAR(72),
	sub_street_level_three VARCHAR(36),
	sub_street_level_two VARCHAR(36),
	sub_street_level_one VARCHAR(36),
	street VARCHAR(36),
	postal_district VARCHAR(36),
	town VARCHAR(36),
	county VARCHAR(36),
	postcode VARCHAR(8),
	scheme_reference INTEGER,
	primary_description_text VARCHAR(60),
	total_area_total_units REAL,
	sub_total REAL,
	total_value REAL,
	adopted_rv REAL,
	list_year INTERGER,
	ba_name VARCHAR(40),
	ba_reference_no VARCHAR(25),
	vo_ref INTERGER,
	from_date CHAR(11),
	to_date CHAR(11),
	scat_code_only CHAR(3),
	unit_of_measurement CHAR(3),
	unadjusted_price REAL,
	FOREIGN KEY(uarn) REFERENCES list_entries(uarn)
);

CREATE TABLE IF NOT EXISTS line_items (
	assessment_reference INTEGER,
	record_type INTERGER,
	line INTEGER,
	floor VARCHAR(50),
	description VARCHAR(240),
	area REAL,
	price REAL,
	value INTEGER,
	FOREIGN KEY (assessment_reference) REFERENCES related_list_entries(assessment_reference)
);

CREATE TABLE IF NOT EXISTS additional_items (
	assessment_reference INTEGER,
	record_type INTERGER,
	other_additional_oa_description VARCHAR(240),
	oa_size REAL,
	oa_price REAL,
	oa_value INTERGER,
	FOREIGN KEY (assessment_reference) REFERENCES related_list_entries(assessment_reference)
);

CREATE TABLE IF NOT EXISTS plant_and_machinery (
	assessment_reference INTEGER,
	record_type INTEGER,
	pm_value INTEGER,
	FOREIGN KEY (assessment_reference) REFERENCES related_list_entries(assessment_reference)
);

CREATE TABLE IF NOT EXISTS car_parks (
	assessment_reference INTEGER,
	record_type INTEGER,
	cp_spaces INTEGER,
	cp_spaces_value INTEGER,
	cp_area INTERGER,
	cp_area_value INTEGER,
	cp_total INTEGER,
	FOREIGN KEY (assessment_reference) REFERENCES related_list_entries(assessment_reference)
);

CREATE TABLE IF NOT EXISTS adjustments (
	assessment_reference INTEGER,
	record_type INTEGER,
	adj_desc VARCHAR(51),
	adj_percent REAL,
	FOREIGN KEY (assessment_reference) REFERENCES related_list_entries(assessment_reference)
);

CREATE TABLE IF NOT EXISTS adjustment_totals (
	assessment_reference INTEGER,
	record_type INTEGER,
	total_before_adj INTERGER,
	total_adj INTEGER,
	FOREIGN KEY (assessment_reference) REFERENCES related_list_entries(assessment_reference)
);
