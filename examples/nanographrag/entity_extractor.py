"""
Local entity extraction using spaCy.

Extracts entities (people, organizations, locations, etc.) and relationships
from text without any external API calls.
"""

import re
import hashlib
from dataclasses import dataclass
from typing import Optional

import spacy
from spacy.tokens import Doc, Span


@dataclass
class Entity:
    """An extracted entity."""
    id: str
    name: str
    entity_type: str
    description: str


@dataclass
class Relationship:
    """A relationship between two entities."""
    source_id: str
    target_id: str
    rel_type: str
    description: str


class EntityExtractor:
    """
    Extract entities and relationships from text using spaCy.

    Uses NER for entity extraction and dependency parsing for relationships.
    """

    # Map spaCy entity types to our types
    ENTITY_TYPE_MAP = {
        "PERSON": "Person",
        "ORG": "Organization",
        "GPE": "Location",
        "LOC": "Location",
        "PRODUCT": "Product",
        "WORK_OF_ART": "Work",
        "EVENT": "Event",
        "LAW": "Law",
        "LANGUAGE": "Language",
        "FAC": "Facility",
        "NORP": "Group",  # Nationalities, religious/political groups
    }

    def __init__(self, model: str = "en_core_web_sm"):
        """
        Initialize extractor with spaCy model.

        Args:
            model: spaCy model name (default: en_core_web_sm, ~12MB)
        """
        try:
            self.nlp = spacy.load(model)
        except OSError:
            # Model not installed, download it
            print(f"Downloading spaCy model '{model}'...")
            spacy.cli.download(model)
            self.nlp = spacy.load(model)

    def _make_id(self, name: str, entity_type: str) -> str:
        """Generate a stable ID for an entity."""
        key = f"{entity_type}:{name.lower()}"
        return hashlib.md5(key.encode()).hexdigest()[:8]

    def _get_entity_context(self, ent: Span, doc: Doc) -> str:
        """Get surrounding context for an entity."""
        start = max(0, ent.start - 5)
        end = min(len(doc), ent.end + 5)
        # Clean up whitespace in context
        context = doc[start:end].text
        return re.sub(r"\s+", " ", context).strip()

    def _clean_name(self, name: str) -> str:
        """Clean entity name for use in queries."""
        # Remove possessive suffix
        name = re.sub(r"'s$", "", name)
        # Remove trailing punctuation
        name = re.sub(r"[.,!?;:]+$", "", name)
        # Replace newlines and excessive whitespace
        name = re.sub(r"\s+", " ", name)
        # Remove leading/trailing whitespace
        name = name.strip()
        return name

    def extract_entities(self, text: str) -> list[Entity]:
        """
        Extract named entities from text.

        Returns:
            List of Entity objects
        """
        doc = self.nlp(text)
        entities = {}

        for ent in doc.ents:
            if ent.label_ not in self.ENTITY_TYPE_MAP:
                continue

            entity_type = self.ENTITY_TYPE_MAP[ent.label_]
            name = self._clean_name(ent.text)

            # Skip very short names
            if len(name) < 2:
                continue

            eid = self._make_id(name, entity_type)

            if eid not in entities:
                entities[eid] = Entity(
                    id=eid,
                    name=name,
                    entity_type=entity_type,
                    description=self._get_entity_context(ent, doc)
                )

        return list(entities.values())

    def extract_relationships(self, text: str, entities: list[Entity]) -> list[Relationship]:
        """
        Extract relationships between entities.

        Uses co-occurrence in sentences and verb patterns.

        Returns:
            List of Relationship objects
        """
        doc = self.nlp(text)
        relationships = []
        entity_map = {e.name.lower(): e for e in entities}

        # Find entities that co-occur in the same sentence
        for sent in doc.sents:
            sent_entities = []
            for ent in sent.ents:
                name_lower = ent.text.strip().lower()
                if name_lower in entity_map:
                    sent_entities.append((ent, entity_map[name_lower]))

            # Create relationships between co-occurring entities
            for i, (span1, ent1) in enumerate(sent_entities):
                for span2, ent2 in sent_entities[i+1:]:
                    if ent1.id == ent2.id:
                        continue

                    # Find verb between entities
                    rel_type = self._find_relationship_type(span1, span2, sent)

                    relationships.append(Relationship(
                        source_id=ent1.id,
                        target_id=ent2.id,
                        rel_type=rel_type,
                        description=sent.text.strip()
                    ))

        return relationships

    def _find_relationship_type(self, span1: Span, span2: Span, sent) -> str:
        """Find the relationship type between two entity spans."""
        # Look for verbs between the entities
        start = min(span1.end, span2.end)
        end = max(span1.start, span2.start)

        verbs = []
        for token in sent:
            if token.i >= start and token.i < end:
                if token.pos_ == "VERB":
                    # Clean verb: only alphanumeric and underscore
                    verb = re.sub(r"[^A-Za-z0-9]", "_", token.lemma_.upper())
                    verb = re.sub(r"_+", "_", verb).strip("_")
                    if verb:
                        verbs.append(verb)

        if verbs:
            return verbs[0] if verbs[0] else "RELATED_TO"

        # Default
        return "RELATED_TO"

    def extract(self, text: str) -> tuple[list[Entity], list[Relationship]]:
        """
        Extract both entities and relationships from text.

        Returns:
            Tuple of (entities, relationships)
        """
        entities = self.extract_entities(text)
        relationships = self.extract_relationships(text, entities)
        return entities, relationships
