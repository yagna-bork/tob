-- INDEXES
CREATE INDEX IF NOT EXISTS idx_list_entries_postcode
ON list_entries(postcode);

CREATE INDEX IF NOT EXISTS idx_related_list_entries_uarn
ON related_list_entries(uarn);

CREATE INDEX IF NOT EXISTS idx_line_items_assessment_reference
ON line_items(assessment_reference);

CREATE INDEX IF NOT EXISTS idx_additional_items_assessment_reference
ON additional_items(assessment_reference);

CREATE INDEX IF NOT EXISTS idx_plant_and_machinery_assessment_reference
ON plant_and_machinery(assessment_reference);

CREATE INDEX IF NOT EXISTS idx_car_parks_assessment_reference
ON car_parks(assessment_reference);

CREATE INDEX IF NOT EXISTS idx_adjustments_assessment_reference
ON adjustments(assessment_reference);

CREATE INDEX IF NOT EXISTS idx_adjustment_totals_assessment_reference
ON adjustment_totals(assessment_reference);
