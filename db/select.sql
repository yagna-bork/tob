-- Queries
SELECT le.uarn, count(rle.assessment_reference) ctr
FROM list_entries le INNER JOIN related_list_entries rle 
	 ON le.uarn = rle.uarn
GROUP BY le.uarn
ORDER BY ctr DESC
LIMIT 10;

SELECT DISTINCT substr(from_date, 4, 3) mon
FROM related_list_entries;

SELECT uarn, MAX(from_date)
FROM related_list_entries
GROUP BY uarn

SELECT le.full_property_identifier, le.primary_secondary_description_code, 
	   le.primary_description_text, le.composite_indicator, le.rateable_value, 
	   rle.assessment_reference
FROM list_entries le
	 INNER JOIN related_list_entries on 
WHERE (number_or_name = '' OR number_or_name LIKE '{} %' 
	   OR number_or_name LIKE '% {}' OR number_or_name LIKE '% {} %') 
	  AND street = '' AND postcode = '';

SELECT assessment_reference
FROM line_items
WHERE uarn in ('', '');

SELECT uarn, MAX(from_date)
FROM related_list_entries
GROUP BY uarn
LIMIT 10;

SELECT srle.assessment_reference, c.description_text, le.primary_description_text, 
	   le.composite_indicator, le.rateable_value, le.uarn
FROM list_entries le
  INNER JOIN related_list_entries rle
  ON le.uarn = rle.uarn AND rle.from_date = (
	SELECT MAX(from_date) FROM related_list_entries WHERE uarn = le.uarn)
  INNER JOIN scat_codes sc
  ON le.scat_code_and_suffix = sc.scat_code_and_suffix
WHERE le.postcode = 'SS8 7AE' and le.street = 'FURTHERWICK ROAD' and (
  le.number_or_name = '52' OR le.number_or_name LIKE '% 52' 
  OR le.number_or_name LIKE '52 %' OR le.number_or_name LIKE '% 52 %'
);
--52, FURTHERWICK ROAD, CANVEY ISLAND, SS8 7AE = Subway lol

CREATE VIEW valid_list_entries AS
SELECT rle.assessment_reference, sc.description_text, le.primary_description_text, 
	   le.composite_indicator, le.rateable_value, le.uarn
FROM list_entries le
  INNER JOIN related_list_entries rle
  ON le.uarn = rle.uarn AND rle.from_date = (
	SELECT MAX(from_date) FROM related_list_entries WHERE uarn = le.uarn)
  INNER JOIN scat_codes sc
  ON le.scat_code_and_suffix = sc.scat_code_and_suffix
WHERE le.postcode = 'SS8 7AE' and le.street = 'FURTHERWICK ROAD' and (
  le.number_or_name = '52' OR le.number_or_name LIKE '% 52' 
  OR le.number_or_name LIKE '52 %' OR le.number_or_name LIKE '% 52 %'
);

--Items
SELECT assessment_reference, floor, description, area, value
FROM line_items
WHERE assessment_reference IN (26418466000,26418464000,26418455000,26418453000)
UNION
SELECT assessment_reference, 'Addtional' floor, oa_description description, 
       oa_size area, oa_value value
FROM additional_items
WHERE assessment_reference IN (26418466000,26418464000,26418455000,26418453000);

--Plant and machinery
SELECT assessment_reference, pm_value
FROM plant_and_machinery
WHERE assessment_reference IN (25128887000,25128888000);

--Parking
SELECT assessment_reference, cp_spaces, cp_area, cp_total
FROM car_parks
WHERE assessment_reference IN (27841034000, 30336030000, 26418543000);

--Building shapes
SELECT inp.grid_row, inp.grid_col
FROM (
	SELECT 21304 grid_row, 16350 grid_col
  ) inp
  LEFT JOIN building_shapes bs
  ON inp.grid_row = bs.grid_row AND inp.grid_col = bs.grid_col
WHERE bs.grid_row IS NULL AND bs.grid_col IS NULL;

INSERT INTO building_shapes VALUES(12345, 12345, :0);

SELECT grid_row, grid_col, data
FROM building_shapes
WHERE grid_row = AND grid_col = ;

--Debugging
SELECT le.uarn, rle.assessment_reference
FROM list_entries le
  INNER JOIN related_list_entries rle
  ON le.uarn = rle.uarn
WHERE le.postcode = 'SS8 7AE' AND le.street = 'FURTHERWICK ROAD' AND le.number_or_name = '54';

SELECT rle.assessment_reference, number_or_name, street, town, firm_name, ai.oa_description, ai.oa_size, ai.oa_value
FROM related_list_entries rle
  INNER JOIN additional_items ai
  ON rle.assessment_reference = ai.assessment_reference
WHERE rle.postcode = 'SS8 7AE';

SELECT rle.assessment_reference, number_or_name, town, street, postcode, pm.pm_value
FROM related_list_entries rle
  INNER JOIN plant_and_machinery pm
  ON rle.assessment_reference = pm.assessment_reference
WHERE town = 'CANVEY ISLAND';

SELECT max(ctr) FROM (SELECT count(*) ctr FROM line_items GROUP BY assessment_reference);

SELECT rle.assessment_reference, number_or_name, town, street, postcode, cp.cp_spaces
FROM related_list_entries rle
  INNER JOIN car_parks cp
  ON rle.assessment_reference = cp.assessment_reference
WHERE rle.postcode = 'SS8 7AE';
