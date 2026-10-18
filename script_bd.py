from sqlalchemy import create_engine, Column, Integer, String, Float, ForeignKey, Date
from sqlalchemy.orm import declarative_base, sessionmaker, relationship
from datetime import date
import os

pasta_atual = os.path.dirname(os.path.abspath(__file__))
caminho_banco = os.path.join(pasta_atual, 'homecare.db')

engine = create_engine(f'sqlite:///{caminho_banco}', echo=False)
Base = declarative_base()

class Paciente(Base):
    __tablename__ = 'pacientes'
    id = Column(Integer, primary_key=True)
    nome = Column(String, nullable=False)
    cpf = Column(String, unique=True)
    data_nascimento = Column(Date)
    
    atendimentos = relationship("Atendimento", back_populates="paciente")

class Profissional(Base):
    __tablename__ = 'profissionais'
    id = Column(Integer, primary_key=True)
    nome = Column(String, nullable=False)
    registro_conselho = Column(String)
    nivel_acesso = Column(String)

class CatalogoItem(Base):
    __tablename__ = 'catalogo_itens'
    id = Column(Integer, primary_key=True)
    codigo = Column(String, unique=True)
    descricao = Column(String, nullable=False)
    valor = Column(Float, nullable=False)
    tipo = Column(String)

class Atendimento(Base):
    __tablename__ = 'atendimentos'
    id = Column(Integer, primary_key=True)
    paciente_id = Column(Integer, ForeignKey('pacientes.id'))
    profissional_id = Column(Integer, ForeignKey('profissionais.id'))
    data_atendimento = Column(Date, nullable=False)
    
    paciente = relationship("Paciente", back_populates="atendimentos")
    itens = relationship("AtendimentoItem", back_populates="atendimento")

class AtendimentoItem(Base):
    __tablename__ = 'atendimento_itens'
    id = Column(Integer, primary_key=True)
    atendimento_id = Column(Integer, ForeignKey('atendimentos.id'))
    catalogo_id = Column(Integer, ForeignKey('catalogo_itens.id'))
    quantidade = Column(Integer, default=1)
    
    atendimento = relationship("Atendimento", back_populates="itens")
    item_catalogo = relationship("CatalogoItem")

Base.metadata.create_all(engine)

Session = sessionmaker(bind=engine)
session = Session()

if session.query(CatalogoItem).count() == 0:
    import pdfplumber
    import re

    nome_arquivo_pdf = 'TABELA DE VALORES GERAL - Planilhas Google.pdf'
    caminho_pdf = os.path.join(pasta_atual, nome_arquivo_pdf)
    
    itens_para_inserir = []
    contador_sem_codigo = 1

    try:
        with pdfplumber.open(caminho_pdf) as pdf:
            for numero_pagina, pagina in enumerate(pdf.pages):
                tabelas = pagina.extract_tables()
                
                for tabela in tabelas:
                    for linha in tabela:
                        if not linha or str(linha[0]).strip() == 'CÓDIGO' or str(linha[1]).strip() == 'TABELA ATENDIMENTO DOMICILIAR':
                            continue

                        codigo_texto = str(linha[0]).strip() if linha[0] else ""
                        descricao_texto = str(linha[1]).strip() if len(linha) > 1 and linha[1] else ""
                        valor_texto = str(linha[2]).strip() if len(linha) > 2 and linha[2] else ""

                        if not descricao_texto or not valor_texto or valor_texto == 'None':
                            continue

                        valor_limpo = re.sub(r'[R$\s]', '', valor_texto).replace('.', '').replace(',', '.')
                        
                        try:
                            valor_float = float(valor_limpo)
                        except ValueError:
                            continue

                        if not codigo_texto or codigo_texto == 'None':
                            codigo_texto = f"S/C_{contador_sem_codigo:03d}"
                            contador_sem_codigo += 1

                        novo_item = CatalogoItem(
                            codigo=codigo_texto,
                            descricao=descricao_texto,
                            valor=valor_float,
                            tipo="Geral"
                        )
                        itens_para_inserir.append(novo_item)
        
        session.add_all(itens_para_inserir)
        session.commit()

    except FileNotFoundError:
        print(f"\nERRO: '{nome_arquivo_pdf}' não foi encontrado")
    except Exception as e:
        print(f"\nOcorreu um erro inesperado durante a leitura do PDF: {e}")

for item in session.query(CatalogoItem).all():
    print(f"Cód: {item.codigo} | {item.descricao} | R$ {item.valor:.2f}")

session.close()

input("\nEnter")